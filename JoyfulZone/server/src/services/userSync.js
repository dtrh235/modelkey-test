'use strict';

const { publishDownlink, waitForMqttReady } = require('./aliyunMqtt');
const userStore = require('./userStore');

const SYNC_DEBOUNCE_MS = 30000;
const inflight = new Map();
const lastSyncAt = new Map();

function latestUpdatedAt(deviceName) {
  return userStore.query(deviceName).reduce((m, u) => Math.max(m, u.updatedAt || 0), 0);
}

/** 请求门锁上报用户列表（不等待回复） */
async function requestSync(deviceName) {
  await waitForMqttReady(8000);
  const id = String(Date.now());
  await publishDownlink({
    cmd: 'sync_users',
    deviceName,
    id,
  });
  lastSyncAt.set(String(deviceName || '1111').trim(), Date.now());
  return { requested: true, id };
}

/** 下发 sync_users 并等待门锁 user_list 写入本地库 */
async function requestSyncAndWait(deviceName, timeoutMs = 8000, force = false) {
  const dn = String(deviceName || '1111').trim();
  const now = Date.now();

  if (!force && inflight.has(dn)) {
    return inflight.get(dn);
  }

  if (!force) {
    const last = lastSyncAt.get(dn) || 0;
    const users = userStore.query(dn);
    const maxAt = latestUpdatedAt(dn);
    if (users.length > 0 && maxAt >= last - 100 && now - last < SYNC_DEBOUNCE_MS) {
      return { ok: true, users, waitedMs: 0, cached: true };
    }
  }

  const job = (async () => {
    await waitForMqttReady(8000);
    const t0 = Date.now();
    const hadUsers = userStore.query(dn).length > 0;

    await publishDownlink({
      cmd: 'sync_users',
      deviceName: dn,
      id: String(t0),
    });
    lastSyncAt.set(dn, t0);

    while (Date.now() - t0 < timeoutMs) {
      await new Promise((r) => setTimeout(r, 150));
      const users = userStore.query(dn);
      const maxAt = latestUpdatedAt(dn);
      if (users.length > 0 && maxAt >= t0 - 100) {
        return { ok: true, users, waitedMs: Date.now() - t0 };
      }
      if (!hadUsers && users.length > 0) {
        return { ok: true, users, waitedMs: Date.now() - t0 };
      }
    }

    const users = userStore.query(dn);
    const maxAt = latestUpdatedAt(dn);
    const fresh = maxAt >= t0 - 100;
    return {
      ok: fresh && users.length > 0,
      users,
      timeout: !fresh,
      stale: !fresh && users.length > 0,
      waitedMs: Date.now() - t0,
    };
  })();

  inflight.set(dn, job);
  try {
    return await job;
  } finally {
    inflight.delete(dn);
  }
}

/** user_ack 丢失时，用 user_list 校验门锁是否已执行 */
async function verifyAccountOp(deviceName, account, op, timeoutMs = 5000) {
  const dn = String(deviceName || '1111').trim();
  const acc = String(account || '').trim();
  if (!acc) {
    return false;
  }
  await requestSyncAndWait(dn, timeoutMs, true);
  const exists = !!userStore.findByAccount(dn, acc);
  if (op === 'add') {
    return exists;
  }
  if (op === 'delete') {
    return !exists;
  }
  return false;
}

module.exports = {
  requestSync,
  requestSyncAndWait,
  verifyAccountOp,
};
