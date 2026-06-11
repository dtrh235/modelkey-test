'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const userRevision = require('./userRevision');

const STORE_PATH = path.join(__dirname, '..', 'data', 'lock_users.json');

function readAll() {
  try {
    const raw = fs.readFileSync(STORE_PATH, 'utf8');
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

function writeAll(list) {
  fs.mkdirSync(path.dirname(STORE_PATH), { recursive: true });
  fs.writeFileSync(STORE_PATH, JSON.stringify(list, null, 2), 'utf8');
}

function accountSortKey(acc) {
  const s = String(acc || '').trim();
  if (/^\d+$/.test(s)) {
    try {
      return BigInt(s);
    } catch {
      return 0n;
    }
  }
  return 0n;
}

function compareUsers(a, b) {
  const ka = accountSortKey(a.acc);
  const kb = accountSortKey(b.acc);
  if (ka < kb) return -1;
  if (ka > kb) return 1;
  return String(a.acc).localeCompare(String(b.acc));
}

function makeId(deviceName, acc) {
  return crypto.createHash('sha1').update(`${deviceName}|${acc}`).digest('hex').slice(0, 16);
}

function normalizeUser(deviceName, raw) {
  const acc = String(raw.acc || raw.account || '').trim();
  if (!acc) {
    return null;
  }
  return {
    id: makeId(deviceName, acc),
    deviceName,
    acc,
    isAdmin: !!(raw.isAdmin ?? raw.is_admin),
    pwd: !!(raw.hasPwd ?? raw.pwd ?? raw.has_pwd ?? true),
    nfc: !!(raw.hasNfc ?? raw.nfc ?? raw.has_nfc),
    fp: !!(raw.hasFp ?? raw.fp ?? raw.has_fp),
    updatedAt: Date.now(),
  };
}

/** 用门锁上报列表覆盖本地缓存（门锁为凭证真源） */
function replaceFromLock(deviceName, users) {
  const dn = deviceName || '1111';
  const incoming = (Array.isArray(users) ? users : [])
    .map((u) => normalizeUser(dn, u))
    .filter(Boolean)
    .sort(compareUsers);

  const others = readAll().filter((u) => u.deviceName !== dn);
  writeAll([...incoming, ...others]);
  userRevision.bump(dn);
  return incoming;
}

function query(deviceName) {
  const dn = deviceName || '1111';
  return readAll().filter((u) => u.deviceName === dn).sort(compareUsers);
}

function upsertLocal(deviceName, partial) {
  const rec = normalizeUser(deviceName, partial);
  if (!rec) {
    return null;
  }
  const list = readAll().filter((u) => !(u.deviceName === rec.deviceName && u.acc === rec.acc));
  list.unshift(rec);
  writeAll(list);
  userRevision.bump(rec.deviceName);
  return rec;
}

function removeLocal(deviceName, account) {
  const acc = String(account || '').trim();
  const list = readAll().filter((u) => !(u.deviceName === deviceName && u.acc === acc));
  writeAll(list);
  userRevision.bump(deviceName);
  return list.length;
}

function findById(deviceName, id) {
  return readAll().find((u) => u.deviceName === deviceName && u.id === id) || null;
}

function findByAccount(deviceName, account) {
  const acc = String(account || '').trim();
  return readAll().find((u) => u.deviceName === deviceName && u.acc === acc) || null;
}

/** 用户库最近变更时间（含删除：用独立 revision，避免删用户后时间戳不前进） */
function getLastChangeAt(deviceName) {
  const dn = deviceName || '1111';
  const users = readAll().filter((u) => u.deviceName === dn);
  let maxAt = userRevision.get(dn);
  for (const u of users) {
    const t = Number(u.updatedAt) || 0;
    if (t > maxAt) {
      maxAt = t;
    }
  }
  return maxAt;
}

module.exports = {
  query,
  replaceFromLock,
  upsertLocal,
  removeLocal,
  findById,
  findByAccount,
  compareUsers,
  getLastChangeAt,
};
