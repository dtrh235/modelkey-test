'use strict';

const express = require('express');
const config = require('../config');
const { getMqttStatus } = require('../services/aliyunMqtt');
const { probeHistoryApi } = require('../services/aliyunOpenApi');
const lockPresence = require('../services/lockPresence');

const router = express.Router();

router.get('/', async (_req, res) => {
  const mqtt = getMqttStatus();
  const aliyunHistory = config.aliyun.openapiConfigured ? await probeHistoryApi() : { ok: false, reason: 'not_configured' };
  res.json({
    ok: true,
    service: 'joyfulzone-server',
    version: '0.1.0',
    time: new Date().toISOString(),
    mqttConfigured: mqtt.configured,
    mqttConnected: mqtt.connected,
    mqttSubscribed: mqtt.subscribed,
    aliyunHistoryApiConfigured: config.aliyun.openapiConfigured,
    aliyunIotInstanceConfigured: config.aliyun.iotInstanceConfigured,
    aliyunHistoryProbe: aliyunHistory,
    mqttTopics: mqtt.topics,
    lockDevice: {
      productKey: config.aliyun.productKey,
      deviceName: config.aliyun.lockDeviceName,
      region: config.aliyun.region,
    },
    serverDevice: {
      productKey: config.aliyun.productKey,
      deviceName: config.aliyun.serverDeviceName || null,
      region: config.aliyun.region,
    },
    lockPresence: lockPresence.getStatus(config.aliyun.lockDeviceName),
  });
});

module.exports = router;
