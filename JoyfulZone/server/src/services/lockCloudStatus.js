'use strict';

const config = require('../config');
const aliyunOpenApi = require('./aliyunOpenApi');
const lockPresence = require('./lockPresence');

/** 后台轮询阿里云设备状态（门锁不再发定时 heartbeat） */
const POLL_INTERVAL_MS = 3000;
/** 单次 API 失败后的最短重试间隔 */
const MIN_RETRY_MS = 1000;

let timer = null;
let pollInFlight = null;
let lastPollAt = 0;

async function pollOnce() {
  if (!config.aliyun.openapiConfigured) {
    return null;
  }
  const now = Date.now();
  if (pollInFlight) {
    return pollInFlight;
  }
  if (now - lastPollAt < MIN_RETRY_MS) {
    return null;
  }
  lastPollAt = now;
  pollInFlight = (async () => {
    const res = await aliyunOpenApi.getLockDeviceStatus();
    if (res.ok && res.online != null) {
      lockPresence.setCloudStatus(config.aliyun.lockDeviceName, res.online, res.ts || Date.now());
      return res;
    }
    if (res.reason) {
      console.warn(`[presence] GetDeviceStatus skip: ${res.reason}`);
    }
    return res;
  })();
  try {
    return await pollInFlight;
  } finally {
    pollInFlight = null;
  }
}

/** App 拉 bind 时可强制刷新一次（带缓存，避免每次 HTTP 都打阿里云） */
async function refreshForApi(force = false) {
  if (!config.aliyun.openapiConfigured) {
    return null;
  }
  if (!force && Date.now() - lastPollAt < MIN_RETRY_MS) {
    return null;
  }
  return pollOnce();
}

function startLockCloudStatusPoller() {
  if (timer || !config.aliyun.openapiConfigured) {
    return;
  }
  pollOnce().catch(() => {});
  timer = setInterval(() => {
    pollOnce().catch(() => {});
  }, POLL_INTERVAL_MS);
  console.log(`[presence] 门锁在线状态：阿里云 GetDeviceStatus 每 ${POLL_INTERVAL_MS / 1000}s`);
}

module.exports = {
  startLockCloudStatusPoller,
  refreshForApi,
  pollOnce,
};
