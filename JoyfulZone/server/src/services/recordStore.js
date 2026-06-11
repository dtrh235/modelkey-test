'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const STORE_PATH = path.join(__dirname, '..', 'data', 'unlock_records.json');

/** 物模型 unlock_method：1密码 2NFC 3指纹 4远程 5临时密码 */
const METHOD_LABEL = {
  1: 'password',
  2: 'nfc',
  3: 'fingerprint',
  4: 'remote',
  5: 'temporary-password',
};

const UNLOCK_TIME_RE = /^(\d{4})[.\-/](\d{2})[.\-/](\d{2})\s+(\d{2}):(\d{2})(?::(\d{2}))?$/;

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

function isValidUnlockTimeText(text) {
  return UNLOCK_TIME_RE.test(String(text || '').trim());
}

/** 解析门锁上报的 unlock_time：YYYY.MM.DD HH:MM；无效返回 null */
function parseUnlockTimeText(text) {
  const s = String(text || '').trim();
  const m = s.match(UNLOCK_TIME_RE);
  if (!m) {
    return null;
  }
  const sec = m[6] ? Number(m[6]) : 0;
  return new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]), Number(m[4]), Number(m[5]), sec).getTime();
}

function formatUnlockTimeText(ts) {
  const d = new Date(ts);
  const pad = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}.${pad(d.getMonth() + 1)}.${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

function normalizeParams(deviceName, params, extra = {}) {
  const account = String(params.unlock_account ?? params.account ?? extra.account ?? '').trim();
  const method = Number(params.unlock_method ?? params.method ?? extra.method ?? 0);
  const device = Number(params.unlock_device ?? params.device ?? extra.device ?? 1);
  const timeText = String(params.unlock_time ?? params.time ?? extra.time ?? '').trim();
  const parsedTs = timeText ? parseUnlockTimeText(timeText) : null;
  const ts = parsedTs != null ? parsedTs : (extra.ts || Date.now());
  const unlockTime = parsedTs != null ? timeText : (timeText || formatUnlockTimeText(ts));
  let normAccount = account || '未知';
  let normMethod = method >= 1 && method <= 5 ? method : 0;
  let normDevice = device === 2 ? 2 : device === 3 ? 3 : 1;
  if (normMethod === 4) {
    normAccount = '0';
    normDevice = 3;
  } else if (normMethod === 5) {
    normAccount = 'temporary account';
  }

  return {
    id: extra.id || crypto.randomUUID(),
    deviceName: deviceName || '1111',
    unlockAccount: normAccount,
    unlockMethod: normMethod,
    unlockDevice: normDevice,
    unlockTime,
    ts,
    ok: extra.ok !== false,
    source: extra.source || 'mqtt',
  };
}

function dedupeKey(rec) {
  return `${rec.deviceName}|${rec.unlockTime}|${rec.unlockAccount}|${rec.unlockMethod}`;
}

/**
 * 写入一条开锁记录（云端全量保存，去重）。
 * @param {string} deviceName
 * @param {object} params 物模型字段或 cmd 信封
 * @param {object} [extra]
 */
function ingest(deviceName, params, extra = {}) {
  const rec = normalizeParams(deviceName, params, extra);
  if (!rec.unlockMethod) {
    return null;
  }
  if (!isValidUnlockTimeText(rec.unlockTime)) {
    return null;
  }

  const list = readAll();
  const key = dedupeKey(rec);
  const exists = list.some((item) => dedupeKey(item) === key);
  if (exists) {
    return null;
  }

  // 30 秒内同账号同方式也视为重复（门锁上报 + 后端补录）
  const nearDup = list.some(
    (item) =>
      item.deviceName === rec.deviceName &&
      item.unlockAccount === rec.unlockAccount &&
      item.unlockMethod === rec.unlockMethod &&
      Math.abs(item.ts - rec.ts) < 30000
  );
  if (nearDup) {
    return null;
  }

  list.unshift(rec);
  writeAll(list);
  return rec;
}

/**
 * App 查询：默认最近 3 个月；后端文件保留全部历史。
 */
function query(deviceName, months = 3) {
  const sinceMs = Date.now() - Math.max(1, months) * 30 * 24 * 60 * 60 * 1000;
  let list = readAll();
  if (deviceName) {
    list = list.filter((item) => item.deviceName === deviceName);
  }
  list = list.filter((item) => item.ts >= sinceMs);
  list.sort((a, b) => b.ts - a.ts);
  return list;
}

function findById(id) {
  return readAll().find((item) => item.id === id) || null;
}

/** 清除指定来源的记录（用于强制刷新阿里云历史前重建） */
function removeBySource(source) {
  const list = readAll().filter((item) => item.source !== source);
  writeAll(list);
  return readAll().length;
}

/** 删除 unlock_time 格式非法的记录（历史合并脏数据） */
function purgeInvalid() {
  const list = readAll();
  const kept = list.filter((item) => isValidUnlockTimeText(item.unlockTime));
  if (kept.length !== list.length) {
    writeAll(kept);
    console.log(`[records] purged ${list.length - kept.length} invalid unlock_time entries`);
  }
  return kept.length;
}

function countToday(deviceName) {
  const start = new Date();
  start.setHours(0, 0, 0, 0);
  const since = start.getTime();
  return readAll().filter((item) => {
    if (deviceName && item.deviceName !== deviceName) return false;
    return item.ts >= since && item.ok !== false;
  }).length;
}

module.exports = {
  ingest,
  query,
  findById,
  countToday,
  removeBySource,
  purgeInvalid,
  isValidUnlockTimeText,
  parseUnlockTimeText,
  formatUnlockTimeText,
  METHOD_LABEL,
};
