'use strict';

const crypto = require('crypto');
const config = require('../config');
const { fetchPropertyHistory, explainApiError } = require('./aliyunOpenApi');
const recordStore = require('./recordStore');

const SYNC_TTL_MS = 5 * 60 * 1000;
/** 同一批物模型上报的 4 个属性应在此时长内到达 */
const CLUSTER_WINDOW_MS = 3000;
/** @type {Map<string, {at:number,count:number}>} */
const lastSync = new Map();

function syncKey(deviceName, months) {
  return `${deviceName}|${months}`;
}

function nearestPoint(list, targetMs, maxDelta) {
  let best = null;
  let bestDelta = Infinity;
  for (const pt of list) {
    const delta = Math.abs(pt.timeMs - targetMs);
    if (delta <= maxDelta && delta < bestDelta) {
      best = pt;
      bestDelta = delta;
    }
  }
  return best;
}

/**
 * 以 unlock_method 变化点为锚，就近匹配 account/device/time（避免跨属性时间窗误配）。
 */
function clusterPropertyStreams(methodList, accountList, deviceList, timeList) {
  const events = [];
  const seen = new Set();

  for (const methodPt of methodList) {
    const method = Number(methodPt.value);
    if (!Number.isFinite(method) || method < 1 || method > 5) {
      continue;
    }

    const anchorMs = methodPt.timeMs;
    const accPt = nearestPoint(accountList, anchorMs, CLUSTER_WINDOW_MS);
    const devPt = nearestPoint(deviceList, anchorMs, CLUSTER_WINDOW_MS);
    const timePt = nearestPoint(timeList, anchorMs, CLUSTER_WINDOW_MS);

    let unlockTime = timePt?.value || '';
    if (!recordStore.isValidUnlockTimeText(unlockTime)) {
      unlockTime = recordStore.formatUnlockTimeText(anchorMs);
    }
    const parsedTs = recordStore.parseUnlockTimeText(unlockTime);
    const ts = parsedTs != null ? parsedTs : anchorMs;

    let account = accPt?.value || '未知';
    if (method === 4) {
      account = '0';
    } else if (method === 5) {
      account = 'temporary account';
    }

    const device = devPt ? Number(devPt.value) : method === 4 ? 3 : 1;
    const dedupe = `${unlockTime}|${account}|${method}`;
    if (seen.has(dedupe)) {
      continue;
    }
    seen.add(dedupe);

    events.push({
      unlock_account: account,
      unlock_method: method,
      unlock_device: device,
      unlock_time: unlockTime,
      ts,
    });
  }

  return events;
}

/**
 * 从阿里云物模型历史同步到本地 recordStore（与 MQTT 实时记录去重合并）。
 */
async function syncUnlockRecords(deviceName, months = 3, force = false) {
  if (!config.aliyun.openapiConfigured) {
    return { imported: 0, total: 0, skipped: 'openapi_not_configured' };
  }
  if (!config.aliyun.iotInstanceConfigured) {
    return {
      imported: 0,
      total: 0,
      skipped: 'iot_instance_id_missing',
      message: explainApiError({ code: 'iot.Sre.IotInstanceNotFound' }),
    };
  }

  if (force) {
    recordStore.removeBySource('aliyun_api');
    recordStore.purgeInvalid();
  }

  const endMs = Date.now();
  const startMs = endMs - Math.max(1, months) * 30 * 24 * 60 * 60 * 1000;
  const dn = deviceName || config.aliyun.lockDeviceName;

  let methodList;
  let accountList;
  let deviceList;
  let timeList;
  try {
    [methodList, accountList, deviceList, timeList] = await Promise.all([
      fetchPropertyHistory('unlock_method', startMs, endMs),
      fetchPropertyHistory('unlock_account', startMs, endMs),
      fetchPropertyHistory('unlock_device', startMs, endMs),
      fetchPropertyHistory('unlock_time', startMs, endMs),
    ]);
  } catch (err) {
    const message = explainApiError(err);
    console.warn(`[aliyun] history sync failed: ${message}`);
    return { imported: 0, total: 0, error: err?.code || err?.data?.Code, message };
  }

  const merged = clusterPropertyStreams(methodList, accountList, deviceList, timeList);
  let imported = 0;

  for (const ev of merged) {
    if (!recordStore.isValidUnlockTimeText(ev.unlock_time)) {
      continue;
    }
    const stableId = crypto
      .createHash('sha1')
      .update(`aliyun|${dn}|${ev.unlock_time}|${ev.unlock_account}|${ev.unlock_method}`)
      .digest('hex');
    const rec = recordStore.ingest(
      dn,
      {
        unlock_account: ev.unlock_account,
        unlock_method: ev.unlock_method,
        unlock_device: ev.unlock_device,
        unlock_time: ev.unlock_time,
      },
      { id: stableId, ts: ev.ts, source: 'aliyun_api' }
    );
    if (rec) {
      imported += 1;
    }
  }

  recordStore.purgeInvalid();

  lastSync.set(syncKey(dn, months), { at: Date.now(), count: imported });
  console.log(
    `[aliyun] history sync device=${dn} months=${months} clusters=${merged.length} imported=${imported} raw_pts=${methodList.length}`
  );

  return { imported, total: merged.length, methodPoints: methodList.length, clusters: merged.length };
}

async function ensureSynced(deviceName, months, force = false) {
  if (!config.aliyun.openapiConfigured) {
    return { synced: false, reason: 'not_configured' };
  }
  const key = syncKey(deviceName, months);
  const prev = lastSync.get(key);
  if (!force && prev && Date.now() - prev.at < SYNC_TTL_MS) {
    return { synced: false, reason: 'ttl', lastAt: prev.at };
  }
  try {
    const result = await syncUnlockRecords(deviceName, months, force);
    if (result.error || result.skipped === 'iot_instance_id_missing') {
      return { synced: false, ...result };
    }
    return { synced: true, ...result };
  } catch (err) {
    const message = explainApiError(err);
    console.warn(`[aliyun] ensureSynced failed: ${message}`);
    return { synced: false, error: err?.code, message };
  }
}

module.exports = {
  syncUnlockRecords,
  ensureSynced,
  clusterPropertyStreams,
};
