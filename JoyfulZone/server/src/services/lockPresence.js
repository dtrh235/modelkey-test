'use strict';

/** 门锁最后一次 terminal/push 上行（业务消息、presence online 等） */
const lastSeen = new Map();
/** MQTT 遗嘱 / presence offline */
const forcedOffline = new Map();
/** 阿里云 GetDeviceStatus 结果 */
const cloudStatus = new Map();

/** 无 OpenAPI 时：多久没任何上行判离线 */
const UPLINK_FALLBACK_TIMEOUT_MS = 30 * 1000;
/** 阿里云 API 结果可信窗口 */
const CLOUD_STATUS_TTL_MS = 8 * 1000;
/** LWT 后至少保持离线此时长，避免 API 仍报 ONLINE 时误恢复 */
const LWT_HOLD_MS = 12 * 1000;

function touchUplink(deviceName) {
  const dn = deviceName || '1111';
  lastSeen.set(dn, Date.now());
}

function touchOnline(deviceName) {
  const dn = deviceName || '1111';
  const now = Date.now();
  forcedOffline.delete(dn);
  lastSeen.set(dn, now);
  cloudStatus.set(dn, { online: true, at: now });
}

function markOffline(deviceName) {
  const dn = deviceName || '1111';
  const now = Date.now();
  forcedOffline.set(dn, now);
  cloudStatus.set(dn, { online: false, at: now });
}

function setCloudStatus(deviceName, online, ts) {
  const dn = deviceName || '1111';
  const at = Number(ts) || Date.now();
  cloudStatus.set(dn, { online: !!online, at });
  if (online) {
    const prev = lastSeen.get(dn) || 0;
    lastSeen.set(dn, Math.max(prev, at));
  }
}

function isLwtActive(dn, now) {
  const at = forcedOffline.get(dn);
  if (!at) {
    return false;
  }
  return (now - at) < LWT_HOLD_MS;
}

function getStatus(deviceName) {
  const dn = deviceName || '1111';
  const now = Date.now();
  const at = lastSeen.get(dn) || 0;
  const cloud = cloudStatus.get(dn);
  const lwt = isLwtActive(dn, now);

  const uplinkRecent = at > 0 && (now - at) < UPLINK_FALLBACK_TIMEOUT_MS;

  if (cloud && (now - cloud.at) < CLOUD_STATUS_TTL_MS) {
    const online = !lwt && (cloud.online || uplinkRecent);
    const seenAt = online ? Math.max(at, cloud.at) : (at || cloud.at || null);
    return {
      online,
      lastSeenAt: seenAt || null,
      offlineMs: seenAt > 0 ? Math.max(0, now - seenAt) : null,
      lwtOffline: lwt,
      source: 'cloud-api',
    };
  }

  if (lwt) {
    return {
      online: false,
      lastSeenAt: at || null,
      offlineMs: at > 0 ? Math.max(0, now - at) : null,
      lwtOffline: true,
      source: 'lwt',
    };
  }

  const online = at > 0 && (now - at) < UPLINK_FALLBACK_TIMEOUT_MS;
  return {
    online,
    lastSeenAt: at || null,
    offlineMs: at > 0 ? Math.max(0, now - at) : null,
    lwtOffline: false,
    source: 'uplink-fallback',
  };
}

module.exports = {
  touch: touchUplink,
  touchUplink,
  touchOnline,
  markOffline,
  setCloudStatus,
  getStatus,
  UPLINK_FALLBACK_TIMEOUT_MS,
  CLOUD_STATUS_TTL_MS,
};
