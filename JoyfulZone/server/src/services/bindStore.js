'use strict';

const fs = require('fs');
const path = require('path');

const STORE_PATH = path.join(__dirname, '..', 'data', 'bindings.json');

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

function findByDevice(deviceName) {
  return readAll().find((item) => item.deviceName === deviceName) || null;
}

function saveBinding(record) {
  const list = readAll().filter((item) => item.deviceName !== record.deviceName);
  list.push(record);
  writeAll(list);
  return record;
}

function removeBinding(deviceName) {
  const list = readAll().filter((item) => item.deviceName !== deviceName);
  writeAll(list);
}

module.exports = {
  findByDevice,
  saveBinding,
  removeBinding,
};
