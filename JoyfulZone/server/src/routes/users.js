'use strict';

const express = require('express');
const config = require('../config');
const bindStore = require('../services/bindStore');
const userStore = require('../services/userStore');
const userSync = require('../services/userSync');
const notifyStore = require('../services/notifyStore');
const { publishDownlink, userCommandWithAck, getMqttStatus } = require('../services/aliyunMqtt');

const router = express.Router();

function boundDevice(req, res) {
  const deviceName = req.body?.deviceName || req.query?.deviceName || config.aliyun.lockDeviceName;
  const binding = bindStore.findByDevice(deviceName);
  if (!binding) {
    res.status(400).json({ ok: false, error: 'NOT_BOUND', message: '请先在 App 中绑定门锁' });
    return null;
  }
  const mqtt = getMqttStatus();
  if (!mqtt.configured || !mqtt.connected) {
    res.status(503).json({ ok: false, error: 'MQTT_NOT_CONNECTED', message: '后端 MQTT 未连接' });
    return null;
  }
  return deviceName;
}

function validAccount(acc) {
  return typeof acc === 'string' && /^\d{1,12}$/.test(acc);
}

function validPassword(pwd) {
  return typeof pwd === 'string' && /^\d{4,10}$/.test(pwd);
}

function notifyUserChange(deviceName, action, account, partial = {}) {
  notifyStore.ingestUserEvent(
    {
      action,
      account,
      isAdmin: partial.isAdmin,
      hasPwd: partial.hasPwd,
      hasNfc: partial.hasNfc,
      hasFp: partial.hasFp,
      id: `app-${Date.now()}`,
    },
    deviceName
  );
}

/** GET /users — 用户列表（默认缓存；refresh=1 向门锁同步） */
router.get('/', async (req, res, next) => {
  try {
    const deviceName = String(req.query.deviceName || config.aliyun.lockDeviceName).trim();
    const refresh = req.query.refresh === '1' || req.query.refresh === 'true';
    const bound = !!bindStore.findByDevice(deviceName);
    const cached = userStore.query(deviceName);

    if (bound && (refresh || cached.length === 0)) {
      await userSync.requestSyncAndWait(deviceName, refresh ? 8000 : 6000, !!refresh);
    }

    const users = userStore.query(deviceName);
    res.json({
      ok: true,
      deviceName,
      total: users.length,
      users,
      synced: refresh || (bound && cached.length === 0),
    });
  } catch (err) {
    next(err);
  }
});

/** GET /users/:id */
router.get('/:id', (req, res) => {
  const deviceName = String(req.query.deviceName || config.aliyun.lockDeviceName).trim();
  const user = userStore.findById(deviceName, req.params.id);
  if (!user) {
    return res.status(404).json({ ok: false, error: 'NOT_FOUND', message: '用户不存在' });
  }
  return res.json({ ok: true, user });
});

/** POST /users — 添加用户并下发门锁 */
router.post('/', async (req, res, next) => {
  let deviceName = null;
  const account = String(req.body.account || '').trim();
  try {
    deviceName = boundDevice(req, res);
    if (!deviceName) {
      return;
    }

    const password = String(req.body.password || '').trim();
    const isAdmin = !!req.body.isAdmin;

    if (!validAccount(account)) {
      return res.status(400).json({ ok: false, error: 'INVALID_ACCOUNT', message: '账号须为 1-12 位数字' });
    }
    if (!validPassword(password)) {
      return res.status(400).json({ ok: false, error: 'INVALID_PASSWORD', message: '密码须为 4-10 位数字' });
    }
    if (userStore.findByAccount(deviceName, account)) {
      return res.status(400).json({ ok: false, error: 'ACCOUNT_EXISTS', message: '账号已存在' });
    }

    const ack = await userCommandWithAck({
      cmd: 'user_add',
      account,
      password,
      isAdmin: isAdmin ? 1 : 0,
    });

    if (!ack.ok) {
      return res.status(400).json({
        ok: false,
        error: 'USER_ADD_REJECTED',
        message: '门锁拒绝添加用户（账号冲突或容量已满）',
        ack,
      });
    }

    const user = userStore.upsertLocal(deviceName, {
      acc: account,
      isAdmin,
      hasPwd: true,
      hasNfc: false,
      hasFp: false,
    });
    notifyUserChange(deviceName, 'add', account, { isAdmin, hasPwd: true });

    res.json({ ok: true, user, message: '用户已添加并同步门锁' });
  } catch (err) {
    if (err.code === 'USER_ACK_TIMEOUT') {
      const verified = await userSync.verifyAccountOp(deviceName, account, 'add');
      if (verified) {
        const user = userStore.findByAccount(deviceName, account) || userStore.upsertLocal(deviceName, {
          acc: account,
          isAdmin,
          hasPwd: true,
          hasNfc: false,
          hasFp: false,
        });
        notifyUserChange(deviceName, 'add', account, { isAdmin, hasPwd: true });
        return res.json({ ok: true, user, recovered: true, message: '用户已添加（门锁已确认）' });
      }
      return res.status(504).json({ ok: false, error: 'USER_ACK_TIMEOUT', message: err.message });
    }
    next(err);
  }
});

/** PUT /users/:account/password */
router.put('/:account/password', async (req, res, next) => {
  try {
    const deviceName = boundDevice(req, res);
    if (!deviceName) {
      return;
    }

    const account = String(req.params.account || '').trim();
    const password = String(req.body.password || '').trim();
    const oldPassword = String(req.body.oldPassword || '').trim();
    if (!validAccount(account) || !validPassword(password)) {
      return res.status(400).json({ ok: false, error: 'INVALID_CRED', message: '账号或密码格式不正确' });
    }

    const prev = userStore.findByAccount(deviceName, account);
    if (prev?.isAdmin) {
      if (!validPassword(oldPassword)) {
        return res.status(400).json({
          ok: false,
          error: 'OLD_PASSWORD_REQUIRED',
          message: '修改管理员开门密码须填写原密码',
        });
      }
    }

    const ack = await userCommandWithAck({
      cmd: 'user_set_password',
      account,
      password,
      ...(prev?.isAdmin ? { oldPassword } : {}),
    });

    if (!ack.ok) {
      return res.status(400).json({
        ok: false,
        error: 'PASSWORD_REJECTED',
        message: prev?.isAdmin ? '原开门密码不正确' : '门锁拒绝修改密码',
        ack,
      });
    }

    const user = userStore.upsertLocal(deviceName, {
      acc: account,
      isAdmin: prev?.isAdmin,
      hasPwd: true,
      hasNfc: prev?.nfc,
      hasFp: prev?.fp,
    });
    notifyUserChange(deviceName, 'password', account, {
      isAdmin: prev?.isAdmin,
      hasPwd: true,
      hasNfc: prev?.nfc,
      hasFp: prev?.fp,
    });

    res.json({ ok: true, user, message: '密码已更新' });
  } catch (err) {
    if (err.code === 'USER_ACK_TIMEOUT') {
      return res.status(504).json({ ok: false, error: 'USER_ACK_TIMEOUT', message: err.message });
    }
    next(err);
  }
});

/** DELETE /users/:account */
router.delete('/:account', async (req, res, next) => {
  let deviceName = null;
  const account = String(req.params.account || '').trim();
  try {
    deviceName = boundDevice(req, res);
    if (!deviceName) {
      return;
    }

    if (!validAccount(account)) {
      return res.status(400).json({ ok: false, error: 'INVALID_ACCOUNT', message: '账号格式不正确' });
    }

    const existing = userStore.findByAccount(deviceName, account);
    if (!existing) {
      return res.status(404).json({ ok: false, error: 'NOT_FOUND', message: '用户不存在' });
    }
    if (existing.isAdmin) {
      return res.status(400).json({ ok: false, error: 'ADMIN_PROTECTED', message: '管理员账号不可删除' });
    }

    const ack = await userCommandWithAck({
      cmd: 'user_delete',
      account,
    });

    if (!ack.ok) {
      return res.status(400).json({
        ok: false,
        error: 'DELETE_REJECTED',
        message: '门锁拒绝删除（可能是唯一管理员）',
        ack,
      });
    }

    userStore.removeLocal(deviceName, account);
    notifyUserChange(deviceName, 'delete', account);
    res.json({ ok: true, message: '用户已删除' });
  } catch (err) {
    if (err.code === 'USER_ACK_TIMEOUT') {
      const verified = await userSync.verifyAccountOp(deviceName, account, 'delete');
      if (verified) {
        userStore.removeLocal(deviceName, account);
        notifyUserChange(deviceName, 'delete', account);
        return res.json({ ok: true, recovered: true, message: '用户已删除（门锁已确认）' });
      }
      return res.status(504).json({ ok: false, error: 'USER_ACK_TIMEOUT', message: err.message });
    }
    next(err);
  }
});

module.exports = router;
