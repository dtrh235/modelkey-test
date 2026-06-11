'use strict';

const express = require('express');
const config = require('../config');
const bindStore = require('../services/bindStore');
const tempPasswordStore = require('../services/tempPasswordStore');
const { bridgeCommandWithAck, publishDownlink, getMqttStatus } = require('../services/aliyunMqtt');

const router = express.Router();

function mqttReady(res) {
  const mqtt = getMqttStatus();
  if (!mqtt.configured) {
    res.status(503).json({
      ok: false,
      error: 'ALIYUN_MQTT_NOT_CONFIGURED',
      message: '后端 MQTT 未配置',
    });
    return false;
  }
  if (!mqtt.connected) {
    res.status(503).json({
      ok: false,
      error: 'MQTT_NOT_CONNECTED',
      message: '后端 MQTT 未连接',
    });
    return false;
  }
  return true;
}

/** GET /temp-passwords — 当前有效临时密码 */
router.get('/', (req, res) => {
  const deviceName = req.query.deviceName || config.aliyun.lockDeviceName;
  tempPasswordStore.pruneExpired();
  res.json({
    ok: true,
    deviceName,
    items: tempPasswordStore.listActive(deviceName),
    validMs: tempPasswordStore.TEMP_VALID_MS,
  });
});

/** POST /temp-passwords — 生成并下发到门锁 */
router.post('/', async (req, res, next) => {
  let tp = null;
  try {
    const deviceName = req.body.deviceName || config.aliyun.lockDeviceName;
    const binding = bindStore.findByDevice(deviceName);
    if (!binding) {
      return res.status(400).json({
        ok: false,
        error: 'NOT_BOUND',
        message: '请先在 App 中绑定门锁',
      });
    }
    if (!mqttReady(res)) {
      return;
    }

    const reserved = Array.isArray(req.body.reservedAccounts) ? req.body.reservedAccounts : [];
    tp = tempPasswordStore.create(deviceName, reserved);

    const ack = await bridgeCommandWithAck(
      {
        cmd: 'set_temp_password',
        account: tp.account,
        password: tp.password,
        validSeconds: Math.floor(tempPasswordStore.TEMP_VALID_MS / 1000),
        expiresAt: tp.expiresAt,
      },
      8000,
      '门锁未在 8 秒内确认临时密码，请确认门锁在线后重试'
    );

    if (!ack.ok) {
      tempPasswordStore.revoke(tp.id);
      return res.status(400).json({
        ok: false,
        error: 'TEMP_PASSWORD_REJECTED',
        message: '门锁拒绝临时密码（可能已满或离线）',
        ack,
      });
    }

    tempPasswordStore.markPushed(tp.id);

    res.json({
      ok: true,
      tempPassword: tp,
      message: '临时密码已生成并下发门锁',
    });
  } catch (err) {
    if (err.code === 'BRIDGE_ACK_TIMEOUT') {
      if (typeof tp !== 'undefined' && tp?.id) {
        tempPasswordStore.revoke(tp.id);
      }
      return res.status(504).json({
        ok: false,
        error: 'TEMP_PASSWORD_ACK_TIMEOUT',
        message: err.message,
      });
    }
    next(err);
  }
});

/** DELETE /temp-passwords/:id — 作废 */
router.delete('/:id', async (req, res, next) => {
  try {
    if (!mqttReady(res)) {
      return;
    }
    const rec = tempPasswordStore.revoke(req.params.id);
    if (!rec) {
      return res.status(404).json({ ok: false, error: 'NOT_FOUND', message: '临时密码不存在' });
    }
    try {
      await publishDownlink({
        cmd: 'revoke_temp_password',
        account: rec.account,
        id: rec.id,
      });
    } catch (err) {
      console.warn('[temp-password] revoke notify failed:', err.message);
    }
    res.json({ ok: true, message: '已作废' });
  } catch (err) {
    next(err);
  }
});

module.exports = router;
