'use strict';

require('dotenv').config({ path: require('path').join(__dirname, '..', '.env') });
const { RPCClient } = require('@alicloud/pop-core');

const BASE = 'http://127.0.0.1:3000';

async function get(path) {
  const r = await fetch(`${BASE}${path}`, { signal: AbortSignal.timeout(60000) });
  const text = await r.text();
  let json;
  try {
    json = JSON.parse(text);
  } catch {
    json = { raw: text.slice(0, 200) };
  }
  return { status: r.status, json };
}

async function post(path, body) {
  const r = await fetch(`${BASE}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(15000),
  });
  const json = await r.json().catch(() => ({}));
  return { status: r.status, json };
}

async function testAliyunApi() {
  const id = process.env.ALIYUN_ACCESS_KEY_ID;
  const secret = process.env.ALIYUN_ACCESS_KEY_SECRET;
  if (!id || !secret) {
    return { ok: false, step: 'aliyun_api', error: 'missing_access_key' };
  }
  const client = new RPCClient({
    accessKeyId: id,
    accessKeySecret: secret,
    endpoint: 'https://iot.cn-shanghai.aliyuncs.com',
    apiVersion: '2018-01-20',
  });
  try {
    const res = await client.request('QueryDevicePropertyData', {
      ProductKey: process.env.ALIYUN_IOT_PRODUCT_KEY || 'k1vtgud0XCS',
      DeviceName: process.env.ALIYUN_LOCK_DEVICE_NAME || '1111',
      Identifier: 'unlock_method',
      StartTime: Date.now() - 90 * 86400000,
      EndTime: Date.now(),
      PageSize: 10,
      Asc: 1,
    });
    const list = res?.Data?.List?.PropertyInfo || [];
    return {
      ok: true,
      step: 'aliyun_api',
      points: list.length,
      sample: list.slice(0, 3),
    };
  } catch (e) {
    return {
      ok: false,
      step: 'aliyun_api',
      code: e.code,
      message: e.message,
    };
  }
}

async function main() {
  const results = [];

  const health = await get('/health');
  results.push({
    step: 'health',
    ok: health.status === 200 && health.json.ok,
    status: health.status,
    mqtt: health.json.mqttConnected,
    historyApi: health.json.aliyunHistoryApiConfigured,
  });

  const status = await get('/devices/status');
  results.push({
    step: 'devices_status',
    ok: status.json.ok,
    mqtt: status.json.serverMqttConnected,
  });

  const bind = await get('/devices/bind?deviceName=1111');
  results.push({
    step: 'bind_query',
    ok: bind.json.ok,
    bound: bind.json.bound,
  });

  results.push(await testAliyunApi());

  const records = await get('/records?deviceName=1111&months=3&refresh=1');
  results.push({
    step: 'records_refresh',
    ok: records.status === 200 && records.json.ok,
    status: records.status,
    total: records.json.total,
    aliyunSync: records.json.aliyunSync,
    error: records.json.message || records.json.error,
  });

  const today = await get('/records/stats/today?deviceName=1111');
  results.push({
    step: 'records_today',
    ok: today.json.ok,
    count: today.json.count,
  });

  const notify = await get('/notifications?deviceName=1111&months=3');
  results.push({
    step: 'notifications',
    ok: notify.json.ok,
    total: notify.json.total,
  });

  const unlock = await post('/devices/unlock', { appAccount: '1001', deviceName: '1111' });
  results.push({
    step: 'unlock_without_bind',
    ok: unlock.json.error === 'NOT_BOUND',
    status: unlock.status,
    error: unlock.json.error,
    note: 'expected NOT_BOUND when not paired',
  });

  const bindTry = await post('/devices/bind', {
    pairCode: '000000',
    appAccount: '1001',
    deviceName: '1111',
  });
  results.push({
    step: 'bind_invalid_code',
    ok: bindTry.json.error === 'BIND_REJECTED' || bindTry.json.error === 'BIND_ACK_TIMEOUT',
    status: bindTry.status,
    error: bindTry.json.error,
    note: 'dummy code; BIND_ACK_TIMEOUT if lock offline',
  });

  console.log(JSON.stringify(results, null, 2));
}

main().catch((e) => {
  console.error('smoke_test_fatal', e.message);
  process.exit(1);
});
