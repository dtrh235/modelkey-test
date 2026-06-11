'use strict';

const fs = require('fs');
const path = require('path');

const REV_PATH = path.join(__dirname, '..', 'data', 'user_revisions.json');

function readAll() {
  try {
    const raw = fs.readFileSync(REV_PATH, 'utf8');
    const parsed = JSON.parse(raw);
    return parsed && typeof parsed === 'object' ? parsed : {};
  } catch {
    return {};
  }
}

function writeAll(map) {
  fs.mkdirSync(path.dirname(REV_PATH), { recursive: true });
  fs.writeFileSync(REV_PATH, JSON.stringify(map, null, 2), 'utf8');
}

function bump(deviceName) {
  const dn = String(deviceName || '1111').trim();
  const map = readAll();
  map[dn] = Date.now();
  writeAll(map);
  return map[dn];
}

function get(deviceName) {
  const dn = String(deviceName || '1111').trim();
  return Number(readAll()[dn]) || 0;
}

module.exports = {
  bump,
  get,
};
