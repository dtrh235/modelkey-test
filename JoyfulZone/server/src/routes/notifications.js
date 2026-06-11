'use strict';

const express = require('express');
const config = require('../config');
const recordStore = require('../services/recordStore');
const notifyStore = require('../services/notifyStore');

const router = express.Router();

const METHOD_LABEL = { 1: '密码', 2: 'NFC', 3: '指纹', 4: '手机', 5: '临时密码' };
const DEVICE_LABEL = { 1: '主屏', 2: '侧门', 3: '手机 App' };

function formatAccount(acc, method) {
  if (acc === '0' && method === 4) return '手机';
  return acc || '未知';
}

function recordToNotify(rec) {
  const method = METHOD_LABEL[rec.unlockMethod] || '未知';
  const device = DEVICE_LABEL[rec.unlockDevice] || '门锁';
  const account = formatAccount(rec.unlockAccount, rec.unlockMethod);
  const rel = new Date(rec.ts).toLocaleString('zh-CN', { hour12: false });
  let desc;
  if (rec.unlockMethod === 4) {
    desc = '手机远程开锁成功';
  } else if (rec.unlockMethod === 5) {
    desc = `访客在${device}使用临时密码开锁`;
  } else {
    desc = `账号 ${account} 在${device}用${method}开锁`;
  }
  return {
    id: `rec-${rec.id}`,
    cat: 'unlock',
    recordId: rec.id,
    title: rec.unlockMethod === 4 ? '远程开锁' : rec.unlockMethod === 5 ? '临时密码开锁' : '开锁提醒',
    desc,
    time: rel,
    ts: rec.ts,
    icon: { 1: 'fa-key', 2: 'fa-nfc-symbol', 3: 'fa-fingerprint', 4: 'fa-mobile-screen', 5: 'fa-clock' }[rec.unlockMethod] || 'fa-lock-open',
    color: { 1: 'bg-amber-50 text-amber-600', 2: 'bg-amber-50 text-amber-600', 3: 'bg-blue-50 text-ycu-blue', 4: 'bg-ycu-teal-light text-ycu-teal-dark', 5: 'bg-orange-50 text-orange-600' }[rec.unlockMethod] || 'bg-ycu-teal-light text-ycu-teal-dark',
  };
}

function alertToNotify(alert) {
  const rel = new Date(alert.ts).toLocaleString('zh-CN', { hour12: false });
  return {
    id: `alert-${alert.id}`,
    cat: 'alert',
    recordId: null,
    title: alert.title || '安全告警',
    desc: alert.desc,
    time: rel,
    ts: alert.ts,
    icon: 'fa-triangle-exclamation',
    color: 'bg-orange-50 text-ycu-sun',
  };
}

const USER_EVENT_ICON = {
  add: 'fa-user-plus',
  delete: 'fa-user-minus',
  password: 'fa-key',
  role: 'fa-user-shield',
  nfc_bind: 'fa-nfc-symbol',
  nfc_delete: 'fa-nfc-symbol',
  fp_bind: 'fa-fingerprint',
  fp_delete: 'fa-fingerprint',
};

function userEventToNotify(ev) {
  const rel = new Date(ev.ts).toLocaleString('zh-CN', { hour12: false });
  return {
    id: `user-${ev.id}`,
    cat: 'device',
    recordId: null,
    account: ev.account,
    action: ev.action,
    title: ev.title || '用户变更',
    desc: ev.desc,
    time: rel,
    ts: ev.ts,
    icon: USER_EVENT_ICON[ev.action] || 'fa-user-gear',
    color: 'bg-violet-50 text-violet-600',
  };
}

/** GET /notifications — 开锁提醒 + 安全告警（默认 3 个月） */
router.get('/', (req, res) => {
  const deviceName = String(req.query.deviceName || config.aliyun.lockDeviceName).trim();
  const months = Math.min(12, Math.max(1, Number(req.query.months) || 3));
  const records = recordStore.query(deviceName, months).map(recordToNotify);
  const alerts = notifyStore.queryAlerts(deviceName, months).map(alertToNotify);
  const userEvents = notifyStore.queryUserEvents(deviceName, months).map(userEventToNotify);
  const notifications = [...records, ...alerts, ...userEvents].sort((a, b) => b.ts - a.ts);

  res.json({
    ok: true,
    deviceName,
    months,
    total: notifications.length,
    notifications,
  });
});

module.exports = router;
