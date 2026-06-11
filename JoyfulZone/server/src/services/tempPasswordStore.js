'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const STORE_PATH = path.join(__dirname, '..', 'data', 'temp_passwords.json');
const TEMP_ACC_LEN = 4;
const TEMP_PWD_LEN = 6;
const TEMP_VALID_MS = 5 * 60 * 1000;

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

function randomDigits(len, noLeadingZero = false) {
  let s = '';
  for (let i = 0; i < len; i += 1) {
    if (i === 0 && noLeadingZero) {
      s += String(Math.floor(Math.random() * 9) + 1);
    } else {
      s += String(Math.floor(Math.random() * 10));
    }
  }
  return s;
}

function takenAccounts(extra = []) {
  const set = new Set(['1', '2', ...extra]);
  for (const item of readAll()) {
    if (!item.used && item.expiresAt > Date.now()) {
      set.add(item.account);
    }
  }
  return set;
}

function create(deviceName, reservedAccounts = []) {
  const taken = takenAccounts(reservedAccounts);
  let account = '';
  for (let i = 0; i < 40; i += 1) {
    const cand = randomDigits(TEMP_ACC_LEN, true);
    if (!taken.has(cand)) {
      account = cand;
      break;
    }
  }
  if (!account) {
    account = randomDigits(TEMP_ACC_LEN, true);
  }
  const password = randomDigits(TEMP_PWD_LEN);
  const now = Date.now();
  const rec = {
    id: crypto.randomUUID(),
    deviceName: deviceName || '1111',
    account,
    password,
    createdAt: now,
    expiresAt: now + TEMP_VALID_MS,
    used: false,
    pushed: false,
  };
  const list = readAll();
  list.unshift(rec);
  writeAll(list);
  return rec;
}

function listActive(deviceName) {
  const now = Date.now();
  return readAll().filter(
    (item) =>
      (!deviceName || item.deviceName === deviceName) &&
      !item.used &&
      item.pushed === true &&
      item.expiresAt > now
  );
}

function markPushed(id) {
  const list = readAll();
  const idx = list.findIndex((item) => item.id === id);
  if (idx < 0) {
    return null;
  }
  list[idx].pushed = true;
  writeAll(list);
  return list[idx];
}

function revoke(id) {
  const list = readAll();
  const idx = list.findIndex((item) => item.id === id);
  if (idx < 0) {
    return null;
  }
  list[idx].used = true;
  writeAll(list);
  return list[idx];
}

/** 门锁上报访客账号已使用 */
function markUsedByAccount(account, deviceName) {
  const acc = String(account || '').trim();
  if (!acc) {
    return null;
  }
  const list = readAll();
  const idx = list.findIndex(
    (item) =>
      item.account === acc &&
      !item.used &&
      (!deviceName || item.deviceName === deviceName)
  );
  if (idx < 0) {
    return null;
  }
  list[idx].used = true;
  writeAll(list);
  return list[idx];
}

function pruneExpired() {
  const now = Date.now();
  const list = readAll();
  let changed = false;
  for (const item of list) {
    if (!item.used && item.expiresAt <= now) {
      item.used = true;
      changed = true;
    }
  }
  if (changed) {
    writeAll(list);
  }
  return list.filter((item) => !item.used && item.expiresAt > now);
}

module.exports = {
  create,
  listActive,
  markPushed,
  revoke,
  markUsedByAccount,
  pruneExpired,
  TEMP_VALID_MS,
};
