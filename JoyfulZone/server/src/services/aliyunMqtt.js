'use strict';

const crypto = require('crypto');
const mqtt = require('mqtt');
const config = require('../config');
const recordStore = require('./recordStore');
const notifyStore = require('./notifyStore');
const tempPasswordStore = require('./tempPasswordStore');
const userStore = require('./userStore');
const bindStore = require('./bindStore');
const lockPresence = require('./lockPresence');
const lockCloudStatus = require('./lockCloudStatus');

/** @type {import('mqtt').MqttClient | null} */
let client = null;
let connected = false;
let subscribed = false;

/** @type {Map<string, { resolve: Function, timer: NodeJS.Timeout }>} */
const pendingAcks = new Map();

function buildMqttPassword(clientId, deviceName, productKey, deviceSecret) {
  const content = `clientId${clientId}deviceName${deviceName}productKey${productKey}`;
  return crypto.createHmac('sha1', deviceSecret).update(content).digest('hex').toUpperCase();
}

function getTopics() {
  const pk = config.aliyun.productKey;
  const dn = config.aliyun.serverDeviceName;
  const lockDn = config.aliyun.lockDeviceName;
  return {
    downlink: `/${pk}/${dn}/user/server/get`,
    uplink: `/${pk}/${dn}/user/server/push`,
    lockPropertyPost: `/sys/${pk}/${lockDn}/thing/event/property/post`,
  };
}

function tryIngestUnlockAlert(topic, msg) {
  if (msg.cmd !== 'unlock_alert') {
    return false;
  }
  const alert = notifyStore.ingestAlert(msg, config.aliyun.lockDeviceName);
  if (alert) {
    console.log(`[mqtt] unlock_alert dev=${alert.device} acc=${alert.lastAccount}`);
  }
  return true;
}

/** 从物模型上报 / terminal/push 桥接 / 云流转封包中提取开锁字段 */
function extractUnlockParams(msg) {
  if (!msg || typeof msg !== 'object') {
    return null;
  }
  if (msg.cmd === 'unlock_record' && msg.unlock_account != null) {
    return msg;
  }
  if (msg.method === 'thing.event.property.post' && msg.params) {
    return msg.params;
  }
  if (msg.params && msg.params.unlock_method != null) {
    return msg.params;
  }
  if (msg.items && typeof msg.items === 'object') {
    const flat = {};
    for (const [key, val] of Object.entries(msg.items)) {
      if (val != null && typeof val === 'object' && 'value' in val) {
        flat[key] = val.value;
      } else {
        flat[key] = val;
      }
    }
    if (flat.unlock_method != null) {
      return flat;
    }
  }
  if (msg.unlock_account != null && msg.unlock_method != null) {
    return msg;
  }
  return null;
}

function tryIngestLockUnbind(msg) {
  if (msg.cmd !== 'lock_unbind') {
    return false;
  }
  const dn = config.aliyun.lockDeviceName;
  bindStore.removeBinding(dn);
  console.log(`[mqtt] lock_unbind device=${dn} -> binding cleared`);
  return true;
}

function tryIngestUserList(msg) {
  if (msg.cmd !== 'user_list' || !Array.isArray(msg.users)) {
    return false;
  }
  const list = userStore.replaceFromLock(config.aliyun.lockDeviceName, msg.users);
  console.log(`[mqtt] user_list imported=${list.length}`);
  return true;
}

function tryResolvePendingUserCommand(msg) {
  if (msg.cmd !== 'user_changed') {
    return false;
  }
  const action = String(msg.action || '').trim();
  const account = String(msg.account || '').trim();
  if (!account) {
    return false;
  }
  for (const [id, pending] of pendingAcks.entries()) {
    if (!pending.userCmd) {
      continue;
    }
    const acc = pending.account || '';
    let matched = false;
    if (pending.userCmd === 'user_add' && action === 'add' && account === acc) {
      matched = true;
    } else if (pending.userCmd === 'user_delete' && action === 'delete' && account === acc) {
      matched = true;
    } else if (pending.userCmd === 'user_set_password' && action === 'password' && account === acc) {
      matched = true;
    }
    if (!matched) {
      continue;
    }
    clearTimeout(pending.timer);
    pendingAcks.delete(id);
    pending.resolve({ ok: true, cmd: 'user_ack', id, via: 'user_changed' });
    console.log(`[mqtt] user op recovered via user_changed cmd=${pending.userCmd} acc=${account}`);
    return true;
  }
  return false;
}

function tryIngestUserChanged(msg) {
  if (msg.cmd !== 'user_changed') {
    return false;
  }
  const lockDn = config.aliyun.lockDeviceName;
  const action = String(msg.action || '').trim();
  const account = String(msg.account || '').trim();
  if (!account) {
    return false;
  }

  if (action === 'delete') {
    userStore.removeLocal(lockDn, account);
  } else {
    userStore.upsertLocal(lockDn, {
      acc: account,
      isAdmin: !!(msg.isAdmin ?? msg.is_admin),
      hasPwd: msg.hasPwd == null ? true : (msg.hasPwd !== 0 && msg.hasPwd !== false && msg.hasPwd !== '0'),
      hasNfc: !!(msg.hasNfc ?? msg.has_nfc),
      hasFp: !!(msg.hasFp ?? msg.has_fp),
    });
  }

  const ev = notifyStore.ingestUserEvent(msg, lockDn);
  console.log(
    `[mqtt] user_changed action=${action} acc=${account}${ev ? '' : ' (dup)'}`
  );
  return true;
}

function tryIngestTempPasswordUsed(msg) {
  if (msg.cmd !== 'temp_password_used') {
    return false;
  }
  const lockDn = config.aliyun.lockDeviceName;
  const account = String(msg.account || '').trim();
  const marked = tempPasswordStore.markUsedByAccount(account, lockDn);
  if (marked) {
    console.log(`[mqtt] temp_password_used acc=${account}`);
  } else {
    console.warn(`[mqtt] temp_password_used miss acc=${account} (未找到有效临时密码)`);
  }
  const rec = recordStore.ingest(
    lockDn,
    {
      unlock_account: 'temporary account',
      unlock_method: 5,
      unlock_device: Number(msg.unlock_device) || 1,
      unlock_time: recordStore.formatUnlockTimeText(Date.now()),
    },
    { source: 'temp_password_used', ts: Date.now() }
  );
  if (rec) {
    console.log(`[mqtt] temp unlock record method=5 time=${rec.unlockTime}`);
  }
  return true;
}

function tryIngestUnlockRecord(topic, msg) {
  const lockDn = config.aliyun.lockDeviceName;

  if (tryIngestTempPasswordUsed(msg)) {
    return true;
  }

  if (tryIngestUnlockAlert(topic, msg)) {
    return true;
  }

  const params = extractUnlockParams(msg);
  if (!params || params.unlock_method == null) {
    return false;
  }

  const rec = recordStore.ingest(lockDn, params, { source: topic });
  if (rec) {
    if (rec.unlockMethod === 5) {
      let guest = String(params.guest_account || params.temp_account || '').trim();
      if (!guest) {
        const actives = tempPasswordStore.listActive(lockDn);
        if (actives.length === 1) {
          guest = actives[0].account;
        }
      }
      if (guest) {
        const marked = tempPasswordStore.markUsedByAccount(guest, lockDn);
        if (marked) {
          console.log(`[mqtt] temp marked used via unlock_record acc=${guest}`);
        }
      }
    }
    console.log(
      `[mqtt] unlock_record acc=${rec.unlockAccount} method=${rec.unlockMethod} dev=${rec.unlockDevice} time=${rec.unlockTime}`
    );
  } else {
    console.log(
      `[mqtt] unlock_record dup/skipped method=${params.unlock_method} acc=${params.unlock_account}`
    );
  }
  return true;
}

function handleUplinkMessage(topic, payload) {
  let msg;
  const raw = payload.toString('utf8');
  try {
    msg = JSON.parse(raw);
  } catch {
    console.log(`[mqtt] uplink non-json topic=${topic} len=${raw.length}`);
    return;
  }

  const lockDn = config.aliyun.lockDeviceName;

  if (msg.cmd === 'presence') {
    if (msg.state === 'offline') {
      lockPresence.markOffline(lockDn);
      console.log('[mqtt] lock presence offline (LWT)');
    } else {
      const wasOnline = lockPresence.getStatus(lockDn).online;
      lockPresence.touchOnline(lockDn);
      if (!wasOnline) {
        console.log('[mqtt] lock presence online');
        repushActiveTempPasswords(lockDn).catch((err) => {
          console.warn('[mqtt] temp password repush:', err.message || err);
        });
      }
    }
    return;
  }

  lockPresence.touchUplink(lockDn);

  if (msg.cmd === 'heartbeat') {
    return;
  }

  if (tryIngestLockUnbind(msg)) {
    return;
  }

  const ackCmds = ['bind_ack', 'unlock_ack', 'unbind_ack', 'user_ack', 'temp_password_ack'];
  if (ackCmds.includes(msg.cmd)) {
    console.log(`[mqtt] ${msg.cmd} id=${msg.id} ok=${msg.ok} raw=${raw.slice(0, 120)}`);
    if (msg.id != null) {
      const pending = pendingAcks.get(String(msg.id));
      if (pending) {
        clearTimeout(pending.timer);
        pendingAcks.delete(String(msg.id));
        pending.resolve(msg);
        return;
      }
    }
  }

  tryResolvePendingUserCommand(msg);

  if (tryIngestUserChanged(msg)) {
    return;
  }

  if (tryIngestUserList(msg)) {
    return;
  }

  if (tryIngestUnlockRecord(topic, msg)) {
    return;
  }

  if (msg.method === 'thing.event.property.post' || msg.cmd === 'unlock_record') {
    console.log(`[mqtt] unlock_record unrecognized shape topic=${topic} raw=${raw.slice(0, 200)}`);
  }
}

function startMqtt() {
  if (!config.aliyun.mqttConfigured) {
    console.log('[mqtt] 未配置 ALIYUN_SERVER_DEVICE_NAME / SECRET，跳过连接');
    return;
  }

  const { productKey, serverDeviceName, serverDeviceSecret, region } = config.aliyun;
  /* 签名用 clientId 前缀；CONNECT 包再拼 securemode（与固件 cloud_aliyun_at 一致） */
  const clientIdSign = `jz-server-${process.pid}`;
  const clientId = `${clientIdSign}|securemode=2,signmethod=hmacsha1|`;
  const username = `${serverDeviceName}&${productKey}`;
  const password = buildMqttPassword(clientIdSign, serverDeviceName, productKey, serverDeviceSecret);
  const host = `${productKey}.iot-as-mqtt.${region}.aliyuncs.com`;

  client = mqtt.connect(`mqtt://${host}:1883`, {
    clientId,
    username,
    password,
    protocolVersion: 4,
    clean: true,
    reconnectPeriod: 5000,
    connectTimeout: 15000,
  });

  function trySubscribe() {
    if (!client || !connected) {
      return;
    }
    const { uplink, lockPropertyPost } = getTopics();
    client.subscribe(uplink, { qos: 0 }, (err) => {
      if (err) {
        subscribed = false;
        console.error(
          `[mqtt] subscribe ${uplink} 失败：请在产品 Topic 列表添加 /user/server/push（订阅权限），见 TOPIC_SETUP.md`
        );
        return;
      }
      subscribed = true;
      console.log(`[mqtt] subscribed ${uplink}`);
    });
    client.subscribe(lockPropertyPost, { qos: 0 }, (err) => {
      if (err) {
        console.warn(
          `[mqtt] subscribe ${lockPropertyPost} 失败：可在云产品流转把物模型上报转发到 server/push，见 TOPIC_SETUP.md`
        );
        return;
      }
      console.log(`[mqtt] subscribed ${lockPropertyPost} (开锁记录)`);
    });
  }

  client.on('connect', () => {
    connected = true;
    console.log('[mqtt] connected');
    trySubscribe();
    setInterval(trySubscribe, 30000);
  });

  client.on('message', handleUplinkMessage);

  client.on('reconnect', () => {
    console.log('[mqtt] reconnecting...');
  });

  client.on('close', () => {
    connected = false;
    subscribed = false;
  });

  client.on('error', (err) => {
    console.error('[mqtt]', err.message);
  });
}

function waitForMqttReady(timeoutMs = 10000) {
  if (connected) {
    return Promise.resolve(true);
  }
  return new Promise((resolve, reject) => {
    const start = Date.now();
    const timer = setInterval(() => {
      if (connected) {
        clearInterval(timer);
        resolve(true);
      } else if (Date.now() - start >= timeoutMs) {
        clearInterval(timer);
        reject(Object.assign(new Error('后端 MQTT 连接超时'), { code: 'MQTT_CONNECT_TIMEOUT' }));
      }
    }, 100);
  });
}

/**
 * 下发 bind 并等待门锁 bind_ack（经云产品流转）。
 * @param {string} pairCode
 * @param {string} appAccount
 * @param {number} [timeoutMs=5000]
 */
function bindWithAck(pairCode, appAccount, timeoutMs = 5000) {
  return new Promise(async (resolve, reject) => {
    if (!config.aliyun.mqttConfigured) {
      reject(Object.assign(new Error('未配置服务器设备 MQTT 凭证'), { code: 'ALIYUN_MQTT_NOT_CONFIGURED' }));
      return;
    }
    if (!client) {
      reject(Object.assign(new Error('MQTT 客户端未初始化'), { code: 'MQTT_NOT_STARTED' }));
      return;
    }

    try {
      await waitForMqttReady(10000);
    } catch (err) {
      reject(err);
      return;
    }

    const id = String(Date.now());
    const { downlink } = getTopics();
    const payload = JSON.stringify({
      cmd: 'bind',
      pairCode,
      appAccount,
      id,
    });

    const timer = setTimeout(() => {
      pendingAcks.delete(id);
      reject(Object.assign(new Error('门锁未响应'), {
        code: 'BIND_ACK_TIMEOUT',
      }));
    }, timeoutMs);

    pendingAcks.set(id, { resolve, timer });

    console.log(`[mqtt] bind publish topic=${downlink} id=${id} pairCode=${pairCode}`);
    client.publish(downlink, payload, { qos: 0 }, (err) => {
      if (err) {
        clearTimeout(timer);
        pendingAcks.delete(id);
        reject(err);
      }
    });
  });
}

/**
 * 下发 unlock 并等待门锁 unlock_ack。
 * @param {string} appAccount
 * @param {number} [timeoutMs=8000]
 */
function unlockWithAck(appAccount, timeoutMs = 8000) {
  return new Promise(async (resolve, reject) => {
    if (!config.aliyun.mqttConfigured) {
      reject(Object.assign(new Error('未配置服务器设备 MQTT 凭证'), { code: 'ALIYUN_MQTT_NOT_CONFIGURED' }));
      return;
    }
    if (!client) {
      reject(Object.assign(new Error('MQTT 客户端未初始化'), { code: 'MQTT_NOT_STARTED' }));
      return;
    }

    try {
      await waitForMqttReady(10000);
    } catch (err) {
      reject(err);
      return;
    }

    const lockDn = config.aliyun.lockDeviceName;
    await lockCloudStatus.refreshForApi(true);
    const presence = lockPresence.getStatus(lockDn);
    if (!presence.online) {
      reject(Object.assign(new Error('门锁云端离线，请等门锁显示在线后再试'), {
        code: 'LOCK_OFFLINE',
      }));
      return;
    }

    const id = String(Date.now());
    const { downlink } = getTopics();
    const payload = JSON.stringify({
      cmd: 'unlock',
      appAccount,
      id,
    });

    const timer = setTimeout(() => {
      pendingAcks.delete(id);
      reject(Object.assign(new Error('门锁未在 8 秒内响应开锁：请确认云流转「服务器→终端」已启动，且门锁串口出现 recv unlock'), {
        code: 'UNLOCK_ACK_TIMEOUT',
      }));
    }, timeoutMs);

    pendingAcks.set(id, { resolve, timer });

    const publishOnce = (label) => {
      console.log(
        `[mqtt] unlock publish ${label} topic=${downlink} id=${id} appAccount=${appAccount} payload=${payload}`
      );
      client.publish(downlink, payload, { qos: 0 }, (err) => {
        if (err) {
          console.error(`[mqtt] unlock publish ${label} fail:`, err.message);
        }
        if (err) {
          clearTimeout(timer);
          pendingAcks.delete(id);
          reject(err);
        }
      });
    };

    /* QoS0 在门锁 MQTT 重连窗口会丢包；短间隔重发提高命中率 */
    publishOnce('try=1');
    setTimeout(() => {
      if (pendingAcks.has(id)) publishOnce('try=2');
    }, 1800);
    setTimeout(() => {
      if (pendingAcks.has(id)) publishOnce('try=3');
    }, 3600);
    setTimeout(() => {
      if (pendingAcks.has(id)) publishOnce('try=4');
    }, 5400);
  });
}

function getMqttStatus() {
  return {
    configured: config.aliyun.mqttConfigured,
    connected,
    subscribed,
    topics: config.aliyun.mqttConfigured ? getTopics() : null,
  };
}

/** 下发桥接指令并等待门锁 ack（id 匹配） */
function bridgeCommandWithAck(payload, timeoutMs = 6000, timeoutMessage = '门锁未响应') {
  return new Promise(async (resolve, reject) => {
    if (!config.aliyun.mqttConfigured) {
      reject(Object.assign(new Error('未配置服务器设备 MQTT 凭证'), { code: 'ALIYUN_MQTT_NOT_CONFIGURED' }));
      return;
    }
    if (!client) {
      reject(Object.assign(new Error('MQTT 客户端未初始化'), { code: 'MQTT_NOT_STARTED' }));
      return;
    }

    try {
      await waitForMqttReady(10000);
    } catch (err) {
      reject(err);
      return;
    }

    const id = String(Date.now());
    const body = { ...payload, id };

    const timer = setTimeout(() => {
      pendingAcks.delete(id);
      reject(Object.assign(new Error(timeoutMessage), { code: 'BRIDGE_ACK_TIMEOUT' }));
    }, timeoutMs);

    const pending = { resolve, timer };
    const cmd = String(payload.cmd || '').trim();
    if (cmd === 'user_add' || cmd === 'user_delete' || cmd === 'user_set_password') {
      pending.userCmd = cmd;
      pending.account = String(payload.account || '').trim();
    }
    pendingAcks.set(id, pending);

    const publishOnce = (label) => {
      console.log(`[mqtt] bridge publish ${label} cmd=${cmd} id=${id}`);
      publishDownlink(body).catch((err) => {
        console.error(`[mqtt] bridge publish ${label} fail:`, err.message);
      });
    };

    publishOnce('try=1');
    setTimeout(() => {
      if (pendingAcks.has(id)) publishOnce('try=2');
    }, 1800);
    setTimeout(() => {
      if (pendingAcks.has(id)) publishOnce('try=3');
    }, 3600);
    setTimeout(() => {
      if (pendingAcks.has(id)) publishOnce('try=4');
    }, 5400);
  });
}

async function repushActiveTempPasswords(deviceName) {
  tempPasswordStore.pruneExpired();
  const items = tempPasswordStore.listActive(deviceName);
  if (!items.length) {
    return;
  }
  for (const tp of items) {
    const remainSec = Math.max(60, Math.floor((tp.expiresAt - Date.now()) / 1000));
    await publishDownlink({
      cmd: 'set_temp_password',
      account: tp.account,
      password: tp.password,
      validSeconds: remainSec,
      id: `repush-${tp.id}`,
    });
  }
  console.log(`[mqtt] temp password repush count=${items.length}`);
}

/** 下发用户类指令并等待 user_ack（或 user_changed 确认） */
async function userCommandWithAck(payload, timeoutMs = 10000) {
  const lockDn = config.aliyun.lockDeviceName;
  await lockCloudStatus.refreshForApi(true);
  const presence = lockPresence.getStatus(lockDn);
  if (!presence.online) {
    throw Object.assign(new Error('门锁云端离线，请等门锁在线后再操作用户'), {
      code: 'LOCK_OFFLINE',
    });
  }
  return bridgeCommandWithAck(payload, timeoutMs, '门锁未在 10 秒内响应用户操作').catch((err) => {
    if (err.code === 'BRIDGE_ACK_TIMEOUT') {
      throw Object.assign(err, { code: 'USER_ACK_TIMEOUT' });
    }
    throw err;
  });
}

/** 经 server/get 下发任意 JSON 指令到门锁 */
function publishDownlink(payload, timeoutMs = 5000) {
  return new Promise(async (resolve, reject) => {
    if (!config.aliyun.mqttConfigured) {
      reject(Object.assign(new Error('未配置服务器设备 MQTT 凭证'), { code: 'ALIYUN_MQTT_NOT_CONFIGURED' }));
      return;
    }
    if (!client) {
      reject(Object.assign(new Error('MQTT 客户端未初始化'), { code: 'MQTT_NOT_STARTED' }));
      return;
    }
    try {
      await waitForMqttReady(timeoutMs);
    } catch (err) {
      reject(err);
      return;
    }
    const { downlink } = getTopics();
    const body = typeof payload === 'string' ? payload : JSON.stringify(payload);
    console.log(`[mqtt] downlink publish topic=${downlink} body=${body.slice(0, 160)}`);
    client.publish(downlink, body, { qos: 0 }, (err) => {
      if (err) {
        reject(err);
      } else {
        resolve({ sent: true });
      }
    });
  });
}

/** 下发 unbind，通知门锁清除「已绑定」状态（保留配对码） */
async function publishUnbind() {
  if (!config.aliyun.mqttConfigured) {
    throw Object.assign(new Error('未配置服务器设备 MQTT 凭证'), { code: 'ALIYUN_MQTT_NOT_CONFIGURED' });
  }
  if (!client) {
    throw Object.assign(new Error('MQTT 客户端未初始化'), { code: 'MQTT_NOT_STARTED' });
  }
  await waitForMqttReady(10000);
  const { downlink } = getTopics();
  const payload = JSON.stringify({ cmd: 'unbind', id: String(Date.now()) });
  return new Promise((resolve, reject) => {
    const publishOnce = (label) => {
      console.log(`[mqtt] unbind publish ${label} topic=${downlink} payload=${payload}`);
      client.publish(downlink, payload, { qos: 0 }, (err) => {
        if (err) {
          console.error(`[mqtt] unbind publish ${label} fail:`, err.message);
        }
      });
    };
    publishOnce('try=1');
    setTimeout(() => publishOnce('try=2'), 1800);
    setTimeout(() => publishOnce('try=3'), 3600);
    setTimeout(() => {
      publishOnce('try=4');
      resolve({ sent: true });
    }, 5400);
  });
}

module.exports = {
  startMqtt,
  bindWithAck,
  unlockWithAck,
  bridgeCommandWithAck,
  userCommandWithAck,
  publishUnbind,
  publishDownlink,
  getMqttStatus,
  waitForMqttReady,
};
