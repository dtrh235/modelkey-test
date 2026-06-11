'use strict';

const RPCClient = require('@alicloud/pop-core').RPCClient;
const config = require('../config');

let client = null;

function getClient() {
  if (!config.aliyun.openapiConfigured) {
    return null;
  }
  if (!client) {
    const { region, accessKeyId, accessKeySecret } = config.aliyun;
    client = new RPCClient({
      accessKeyId,
      accessKeySecret,
      endpoint: `https://iot.${region}.aliyuncs.com`,
      apiVersion: '2018-01-20',
    });
  }
  return client;
}

function iotBaseParams() {
  const { productKey, lockDeviceName, iotInstanceId } = config.aliyun;
  const params = {
    ProductKey: productKey,
    DeviceName: lockDeviceName,
  };
  if (iotInstanceId) {
    params.IotInstanceId = iotInstanceId;
  }
  return params;
}

/** 将 PopCore 错误转为可读说明 */
function explainApiError(err) {
  const code = err?.code || err?.data?.Code || '';
  if (code === 'iot.common.RamActionPermissionDeny') {
    return 'RAM 用户缺少物联网读权限，请添加 AliyunIOTFullAccess';
  }
  if (code === 'iot.Sre.IotInstanceNotFound' || code === 'iot.prod.NotExistedProduct') {
    return '请在 .env 配置 ALIYUN_IOT_INSTANCE_ID（物联网平台控制台 → 实例概览 → 实例 ID）';
  }
  return err?.message || code || '阿里云 API 调用失败';
}

/**
 * 分页拉取单属性历史（毫秒时间戳）。
 * @returns {Promise<Array<{timeMs:number,value:string}>>}
 */
async function fetchPropertyHistory(identifier, startMs, endMs) {
  const rpc = getClient();
  if (!rpc) {
    return [];
  }
  const all = [];
  let cursor = startMs;

  for (let page = 0; page < 200; page += 1) {
    const res = await rpc.request('QueryDevicePropertyData', {
      ...iotBaseParams(),
      Identifier: identifier,
      StartTime: cursor,
      EndTime: endMs,
      PageSize: 50,
      Asc: 1,
    });

    const list = res?.Data?.List?.PropertyInfo || [];
    for (const item of list) {
      const timeMs = Number(item.Time);
      if (!Number.isFinite(timeMs)) {
        continue;
      }
      all.push({ timeMs, value: String(item.Value ?? '').trim() });
    }

    if (!res?.Data?.NextValid || !res?.Data?.NextTime) {
      break;
    }
    const next = Number(res.Data.NextTime);
    if (!Number.isFinite(next) || next <= cursor || next >= endMs) {
      break;
    }
    cursor = next;
  }

  return all;
}

/** 探测 OpenAPI 是否可查询物模型历史 */
async function probeHistoryApi() {
  if (!config.aliyun.openapiConfigured) {
    return { ok: false, reason: 'access_key_not_configured' };
  }
  if (!config.aliyun.iotInstanceConfigured) {
    return { ok: false, reason: 'iot_instance_id_missing', message: explainApiError({ code: 'iot.Sre.IotInstanceNotFound' }) };
  }
  try {
    const endMs = Date.now();
    const startMs = endMs - 7 * 86400000;
    const list = await fetchPropertyHistory('unlock_method', startMs, endMs);
    return { ok: true, samplePoints: list.length };
  } catch (err) {
    return { ok: false, code: err?.code || err?.data?.Code, message: explainApiError(err) };
  }
}

/**
 * 查询门锁 MQTT 会话是否在线（平台侧，不依赖门锁发 heartbeat）。
 * @returns {Promise<{ok:boolean, online:boolean|null, status:string, ts:number|null, reason?:string}>}
 */
async function getLockDeviceStatus() {
  const rpc = getClient();
  if (!rpc) {
    return { ok: false, online: null, status: 'UNKNOWN', ts: null, reason: 'access_key_not_configured' };
  }
  try {
    const res = await rpc.request('GetDeviceStatus', iotBaseParams());
    const status = String(res?.Data?.Status || '').toUpperCase();
    const ts = Number(res?.Data?.Timestamp) || Date.now();
    const online = status === 'ONLINE';
    return { ok: true, online, status, ts };
  } catch (err) {
    return {
      ok: false,
      online: null,
      status: 'UNKNOWN',
      ts: null,
      reason: explainApiError(err),
      code: err?.code || err?.data?.Code,
    };
  }
}

module.exports = {
  getClient,
  fetchPropertyHistory,
  probeHistoryApi,
  getLockDeviceStatus,
  explainApiError,
};
