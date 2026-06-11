'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const STORE_PATH = path.join(__dirname, '..', 'data', 'unlock_alerts.json');
const USER_EVENTS_PATH = path.join(__dirname, '..', 'data', 'user_events.json');

const USER_ACTION_META = {
  add: { title: '新用户已添加', desc: (acc) => `门锁已添加账号 ${acc}` },
  delete: { title: '用户已删除', desc: (acc) => `门锁已删除账号 ${acc}` },
  password: { title: '开门密码已修改', desc: (acc) => `账号 ${acc} 的密码已更新` },
  role: { title: '用户权限已变更', desc: (acc) => `账号 ${acc} 的管理员权限已调整` },
  nfc_bind: { title: '门禁卡已绑定', desc: (acc) => `账号 ${acc} 已绑定 NFC 卡` },
  nfc_delete: { title: '门禁卡已解除', desc: (acc) => `账号 ${acc} 的 NFC 卡已删除` },
  fp_bind: { title: '指纹已录入', desc: (acc) => `账号 ${acc} 已录入指纹` },
  fp_delete: { title: '指纹已删除', desc: (acc) => `账号 ${acc} 的指纹已删除` },
};

const DEVICE_LABEL = { 1: '门锁主屏', 2: '侧门从机', 3: '手机 App' };

function readAll() {
  try {
    const raw = fs.readFileSync(STORE_PATH, 'utf8');
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

function writeAll(list) {
  fs.mkdirSync(path.dirname(STORE_PATH), { recursive: true });
  fs.writeFileSync(STORE_PATH, JSON.stringify(list, null, 2), 'utf8');
}

function ingestAlert(msg, deviceName) {
  const device = Number(msg.device ?? msg.unlock_device ?? 1);
  const failCount = Number(msg.failCount ?? 5);
  const lastAccount = String(msg.lastAccount ?? msg.last_account ?? '').trim();
  const ts = Date.now();
  const rec = {
    id: String(msg.id || crypto.randomUUID()),
    deviceName: deviceName || '1111',
    device,
    failCount,
    lastAccount,
    ts,
    title: '安全告警',
    desc: `${DEVICE_LABEL[device] || '门锁'}连续${failCount}次密码错误，已锁定1分钟`,
  };
  const list = readAll();
  const dup = list.some(
    (item) => item.device === rec.device && Math.abs(item.ts - rec.ts) < 5000
  );
  if (dup) return null;
  list.unshift(rec);
  writeAll(list);
  return rec;
}

function queryAlerts(deviceName, months = 3) {
  const sinceMs = Date.now() - Math.max(1, months) * 30 * 24 * 60 * 60 * 1000;
  let list = readAll();
  if (deviceName) {
    list = list.filter((item) => item.deviceName === deviceName);
  }
  return list.filter((item) => item.ts >= sinceMs).sort((a, b) => b.ts - a.ts);
}

function readUserEvents() {
  try {
    const raw = fs.readFileSync(USER_EVENTS_PATH, 'utf8');
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

function writeUserEvents(list) {
  fs.mkdirSync(path.dirname(USER_EVENTS_PATH), { recursive: true });
  fs.writeFileSync(USER_EVENTS_PATH, JSON.stringify(list, null, 2), 'utf8');
}

function ingestUserEvent(msg, deviceName) {
  const action = String(msg.action || 'update').trim();
  const account = String(msg.account || '').trim();
  if (!account) {
    return null;
  }
  const meta = USER_ACTION_META[action] || {
    title: '用户资料已更新',
    desc: (acc) => `门锁用户 ${acc} 信息已变更`,
  };
  const ts = Date.now();
  const rec = {
    id: String(msg.id || crypto.randomUUID()),
    deviceName: deviceName || '1111',
    action,
    account,
    isAdmin: !!(msg.isAdmin ?? msg.is_admin),
    hasPwd: msg.hasPwd !== 0 && msg.hasPwd !== false && msg.hasPwd !== '0',
    hasNfc: !!(msg.hasNfc ?? msg.has_nfc),
    hasFp: !!(msg.hasFp ?? msg.has_fp),
    ts,
    title: meta.title,
    desc: meta.desc(account),
  };
  const list = readUserEvents();
  const dup = list.some(
    (item) =>
      item.account === rec.account &&
      item.action === rec.action &&
      Math.abs(item.ts - rec.ts) < 8000
  );
  if (dup) {
    return null;
  }
  list.unshift(rec);
  writeUserEvents(list.slice(0, 500));
  return rec;
}

function queryUserEvents(deviceName, months = 3) {
  const sinceMs = Date.now() - Math.max(1, months) * 30 * 24 * 60 * 60 * 1000;
  let list = readUserEvents();
  if (deviceName) {
    list = list.filter((item) => item.deviceName === deviceName);
  }
  return list.filter((item) => item.ts >= sinceMs).sort((a, b) => b.ts - a.ts);
}

module.exports = {
  ingestAlert,
  ingestUserEvent,
  queryAlerts,
  queryUserEvents,
  DEVICE_LABEL,
  USER_ACTION_META,
};
