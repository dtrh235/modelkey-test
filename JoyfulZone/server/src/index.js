'use strict';

const express = require('express');
const cors = require('cors');
const config = require('./config');
const { startMqtt } = require('./services/aliyunMqtt');
const { startLockCloudStatusPoller } = require('./services/lockCloudStatus');
const healthRouter = require('./routes/health');
const devicesRouter = require('./routes/devices');
const recordsRouter = require('./routes/records');
const notificationsRouter = require('./routes/notifications');
const tempPasswordsRouter = require('./routes/tempPasswords');
const usersRouter = require('./routes/users');

const app = express();

app.use(cors());
app.use(express.json({ limit: '64kb' }));

app.use('/health', healthRouter);
app.use('/devices', devicesRouter);
app.use('/records', recordsRouter);
app.use('/notifications', notificationsRouter);
app.use('/temp-passwords', tempPasswordsRouter);
app.use('/users', usersRouter);

app.use((_req, res) => {
  res.status(404).json({ ok: false, error: 'NOT_FOUND' });
});

app.use((err, _req, res, _next) => {
  console.error('[server]', err.message || err);
  res.status(500).json({
    ok: false,
    error: 'INTERNAL_ERROR',
    message: err.message || '服务器内部错误',
  });
});

startMqtt();
startLockCloudStatusPoller();

app.listen(config.port, () => {
  console.log(`JoyfulZone server listening on http://127.0.0.1:${config.port}`);
  console.log('  GET  /health');
  console.log('  GET  /devices/status');
  console.log('  POST /devices/bind   (MQTT 桥接，等待 bind_ack 最多 5s)');
  console.log('  POST /devices/unlock (MQTT 桥接，等待 unlock_ack 最多 8s)');
  console.log('  GET  /records?months=3 (App 开锁记录，默认最近 3 个月)');
  console.log('  GET  /notifications?months=3 (App 通知中心)');
  console.log('  POST /temp-passwords (生成访客临时密码并下发门锁)');
  console.log('  GET  /users?refresh=1 (用户管理，与门锁同步)');
  if (!config.aliyun.openapiConfigured) {
    console.log('  提示: 配置 ALIYUN_ACCESS_KEY_ID/SECRET 后可用阿里云 API 判断门锁在线（推荐）');
    console.log('        未配置时仅靠 presence/业务上行，空闲约 90s 后会显示离线');
  } else if (!config.aliyun.iotInstanceConfigured) {
    console.log('  提示: 配置 ALIYUN_IOT_INSTANCE_ID（控制台实例概览）以拉取物模型历史');
  }
  if (!config.aliyun.mqttConfigured) {
    console.log('  (MQTT 未配置：复制 .env.example → .env 并填写服务器设备 DeviceSecret)');
  }
});
