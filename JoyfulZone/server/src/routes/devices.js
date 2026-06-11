'use strict';

const express = require('express');
const config = require('../config');
const { bindWithAck, unlockWithAck, publishUnbind, getMqttStatus } = require('../services/aliyunMqtt');
const bindStore = require('../services/bindStore');
const recordStore = require('../services/recordStore');
const aliyunHistory = require('../services/aliyunHistory');
const userSync = require('../services/userSync');
const lockPresence = require('../services/lockPresence');
const lockCloudStatus = require('../services/lockCloudStatus');
const userStore = require('../services/userStore');

const router = express.Router();

/** GET /devices/status — 门锁在线状态（无 AccessKey，仅报告后端 MQTT） */
router.get('/status', (_req, res) => {
  const mqtt = getMqttStatus();
  res.json({
    ok: true,
    source: 'mqtt-bridge',
    productKey: config.aliyun.productKey,
    deviceName: config.aliyun.lockDeviceName,
    serverMqttConnected: mqtt.connected,
    serverMqttConfigured: mqtt.configured,
    online: null,
    status: 'UNKNOWN',
    message: mqtt.connected
      ? '后端 MQTT 已连接；门锁在线请通过配对 bind_ack 判断'
      : '后端 MQTT 未连接，请检查 .env 与 npm start',
  });
});

/** GET /devices/bind — 查询本机记录的 App 绑定 */
router.get('/bind', async (req, res) => {
  const deviceName = req.query.deviceName || config.aliyun.lockDeviceName;
  await lockCloudStatus.refreshForApi(true);
  const record = bindStore.findByDevice(deviceName);
  const presence = lockPresence.getStatus(deviceName);
  res.json({
    ok: true,
    bound: !!record,
    binding: record,
    lockOnline: presence.online,
    lockPresenceSource: presence.source || 'unknown',
    lockLastSeenAt: presence.lastSeenAt,
    lockOfflineMs: presence.offlineMs,
    lwtOffline: presence.lwtOffline === true,
    lockUsersUpdatedAt: userStore.getLastChangeAt(deviceName),
  });
});

/** POST /devices/bind — App 提交配对码，经 MQTT 桥接下发并等待 bind_ack */
router.post('/bind', async (req, res, next) => {
  try {
    const pairCode = String(req.body.pairCode || '').trim();
    const appAccount = String(req.body.appAccount || '1001').trim();
    const deviceName = req.body.deviceName || config.aliyun.lockDeviceName;
    const productKey = config.aliyun.productKey;

    if (!/^\d{6}$/.test(pairCode)) {
      return res.status(400).json({ ok: false, error: 'INVALID_PAIR_CODE', message: '请输入 6 位配对码' });
    }

    const mqtt = getMqttStatus();
    if (!mqtt.configured) {
      return res.status(503).json({
        ok: false,
        error: 'ALIYUN_MQTT_NOT_CONFIGURED',
        message: '请在 JoyfulZone/server/.env 配置 ALIYUN_SERVER_DEVICE_NAME / SECRET 并重启后端',
      });
    }

    if (!mqtt.connected) {
      return res.status(503).json({
        ok: false,
        error: 'MQTT_NOT_CONNECTED',
        message: '后端 MQTT 未连接，请确认服务器设备凭证正确且已激活',
      });
    }

    if (!mqtt.subscribed) {
      return res.status(503).json({
        ok: false,
        error: 'MQTT_NOT_SUBSCRIBED',
        message: '后端未订阅 server/push，请在产品 Topic 列表添加自定义 Topic 并赋予订阅权限（见 server/TOPIC_SETUP.md）',
      });
    }

    const ack = await bindWithAck(pairCode, appAccount, 5000);

    if (!ack.ok) {
      return res.status(400).json({
        ok: false,
        error: 'BIND_REJECTED',
        message: '配对码错误或已过期，请在门锁 App配对 页确认后重试',
        ack,
      });
    }

    const binding = bindStore.saveBinding({
      productKey,
      deviceName,
      appAccount,
      boundAt: Date.now(),
    });

    if (config.aliyun.openapiConfigured) {
      aliyunHistory.ensureSynced(deviceName, 3, true).catch((err) => {
        console.warn('[bind] aliyun history sync failed:', err.message);
      });
    }

    userSync.requestSyncAndWait(deviceName, 6000).then((r) => {
      if (r.timeout) {
        console.warn(`[bind] user sync timeout waited=${r.waitedMs}ms`);
      } else {
        console.log(`[bind] user sync ok count=${r.users.length} waited=${r.waitedMs}ms`);
      }
    }).catch((err) => {
      console.warn('[bind] user sync failed:', err.message);
    });

    res.json({
      ok: true,
      sent: true,
      bound: true,
      productKey,
      deviceName,
      appAccount,
      binding,
      ack,
      message: '配对成功，门锁已绑定',
    });
  } catch (err) {
    if (err.code === 'BIND_ACK_TIMEOUT') {
      return res.status(504).json({
        ok: false,
        error: 'BIND_ACK_TIMEOUT',
        message: err.message,
      });
    }
    if (err.code === 'MQTT_CONNECT_TIMEOUT' || err.code === 'MQTT_NOT_STARTED') {
      return res.status(503).json({
        ok: false,
        error: err.code,
        message: err.message,
      });
    }
    next(err);
  }
});

/** POST /devices/unlock — App 远程开锁，经 MQTT 桥接并等待 unlock_ack */
router.post('/unlock', async (req, res, next) => {
  try {
    const appAccount = String(req.body.appAccount || '1001').trim();
    const deviceName = req.body.deviceName || config.aliyun.lockDeviceName;
    const binding = bindStore.findByDevice(deviceName);

    if (!binding) {
      return res.status(400).json({
        ok: false,
        error: 'NOT_BOUND',
        message: '请先在 App 中绑定门锁',
      });
    }

    if (binding.appAccount && appAccount !== binding.appAccount) {
      return res.status(403).json({
        ok: false,
        error: 'ACCOUNT_MISMATCH',
        message: '当前账号无权操作此门锁',
      });
    }

    const mqtt = getMqttStatus();
    if (!mqtt.configured) {
      return res.status(503).json({
        ok: false,
        error: 'ALIYUN_MQTT_NOT_CONFIGURED',
        message: '后端 MQTT 未配置',
      });
    }
    if (!mqtt.connected) {
      return res.status(503).json({
        ok: false,
        error: 'MQTT_NOT_CONNECTED',
        message: '后端 MQTT 未连接',
      });
    }
    if (!mqtt.subscribed) {
      return res.status(503).json({
        ok: false,
        error: 'MQTT_NOT_SUBSCRIBED',
        message: '后端未订阅 server/push',
      });
    }

    const ack = await unlockWithAck(appAccount, 8000);

    if (!ack.ok) {
      return res.status(400).json({
        ok: false,
        error: 'UNLOCK_REJECTED',
        message: '门锁拒绝远程开锁：门锁未记住绑定，请在门锁「App配对」页重新输入 6 位配对码',
        ack,
      });
    }

    recordStore.ingest(deviceName, {
      unlock_account: '0',
      unlock_method: 4,
      unlock_device: 3,
      unlock_time: recordStore.formatUnlockTimeText(Date.now()),
    }, { source: 'unlock_api', ts: Date.now() });

    res.json({
      ok: true,
      sent: true,
      unlocked: true,
      deviceName,
      appAccount,
      ack,
      message: '开锁成功',
    });
  } catch (err) {
    if (err.code === 'UNLOCK_ACK_TIMEOUT') {
      return res.status(504).json({
        ok: false,
        error: 'UNLOCK_ACK_TIMEOUT',
        message: err.message,
      });
    }
    if (err.code === 'MQTT_CONNECT_TIMEOUT' || err.code === 'MQTT_NOT_STARTED') {
      return res.status(503).json({
        ok: false,
        error: err.code,
        message: err.message,
      });
    }
    next(err);
  }
});

/** DELETE /devices/bind — App 解绑：清后端记录并通知门锁清除「已绑定」 */
router.delete('/bind', async (req, res) => {
  const deviceName = req.body?.deviceName || config.aliyun.lockDeviceName;
  bindStore.removeBinding(deviceName);
  try {
    const mqtt = getMqttStatus();
    if (mqtt.configured && mqtt.connected) {
      await publishUnbind();
    }
  } catch (err) {
    console.warn('[devices] unbind notify lock failed:', err.message || err);
  }
  res.json({ ok: true, message: '已解除绑定' });
});

/** GET /devices/:deviceName/status */
router.get('/:deviceName/status', (req, res) => {
  const mqtt = getMqttStatus();
  res.json({
    ok: true,
    source: 'mqtt-bridge',
    productKey: config.aliyun.productKey,
    deviceName: req.params.deviceName,
    serverMqttConnected: mqtt.connected,
    online: null,
    status: 'UNKNOWN',
  });
});

/** GET /devices — 当前单锁配置 */
router.get('/', (_req, res) => {
  res.json({
    ok: true,
    devices: [
      {
        productKey: config.aliyun.productKey,
        deviceName: config.aliyun.lockDeviceName,
        label: 'Host 主门锁',
        statusUrl: '/devices/status',
      },
    ],
  });
});

module.exports = router;
