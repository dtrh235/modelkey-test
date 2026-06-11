'use strict';

require('dotenv').config();

const config = {
  port: Number(process.env.PORT) || 3000,
  aliyun: {
    region: process.env.ALIYUN_IOT_REGION || 'cn-shanghai',
    productKey: process.env.ALIYUN_IOT_PRODUCT_KEY || 'k1vtgud0XCS',
    lockDeviceName: process.env.ALIYUN_LOCK_DEVICE_NAME || '1111',
    serverDeviceName: process.env.ALIYUN_SERVER_DEVICE_NAME || '',
    serverDeviceSecret: process.env.ALIYUN_SERVER_DEVICE_SECRET || '',
    accessKeyId: process.env.ALIYUN_ACCESS_KEY_ID || '',
    accessKeySecret: process.env.ALIYUN_ACCESS_KEY_SECRET || '',
    /** 新版公共实例/企业版必填，控制台「实例概览」中的实例 ID，如 iot-cn-xxxxx */
    iotInstanceId: (process.env.ALIYUN_IOT_INSTANCE_ID || '').trim(),
  },
};

config.aliyun.mqttConfigured =
  config.aliyun.serverDeviceName.length > 0 &&
  config.aliyun.serverDeviceSecret.length > 0;

config.aliyun.openapiConfigured =
  config.aliyun.accessKeyId.length > 0 && config.aliyun.accessKeySecret.length > 0;

config.aliyun.iotInstanceConfigured = config.aliyun.iotInstanceId.length > 0;

module.exports = config;
