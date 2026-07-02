'use strict';

/**
 * RS485 帧编解码（与 Host/Client app_rs485_proto.c 一致）
 * 用于无硬件时验证主从 0x10 开锁通知帧格式。
 */

const SOF0 = 0xa5;
const SOF1 = 0x5a;
const HDR = 8;
const CRC_LEN = 2;
const CMD_SLAVE_UNLOCK = 0x10;

function crc16Modbus(buf, off, len) {
  let crc = 0xffff;
  for (let i = off; i < off + len; i += 1) {
    crc ^= buf[i];
    for (let b = 0; b < 8; b += 1) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xa001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc & 0xffff;
}

/** @returns {Buffer} */
function buildFrame(dst, src, seq, cmd, payload) {
  const pl = payload ? Buffer.from(payload) : Buffer.alloc(0);
  const total = HDR + pl.length + CRC_LEN;
  const frame = Buffer.alloc(total);
  frame[0] = SOF0;
  frame[1] = SOF1;
  frame[2] = dst & 0xff;
  frame[3] = src & 0xff;
  frame[4] = seq & 0xff;
  frame[5] = cmd & 0xff;
  frame[6] = pl.length & 0xff;
  frame[7] = (pl.length >> 8) & 0xff;
  pl.copy(frame, HDR);
  const crc = crc16Modbus(frame, 2, HDR - 2 + pl.length);
  frame[HDR + pl.length] = crc & 0xff;
  frame[HDR + pl.length + 1] = (crc >> 8) & 0xff;
  return frame;
}

function parseUnlockNotify(payload) {
  if (payload.length === 15) {
    const acc = payload.slice(2, 14).toString('utf8').replace(/\0/g, '');
    return {
      method_id: payload[0],
      device_id: payload[1],
      account: acc,
    };
  }
  if (payload.length === 14) {
    const acc = payload.slice(1, 13).toString('utf8').replace(/\0/g, '');
    return { method_id: payload[0], device_id: 2, account: acc };
  }
  return null;
}

function buildSlaveUnlockNotify(account, methodId = 1, deviceId = 2) {
  const acc = String(account || '').slice(0, 12);
  const payload = Buffer.alloc(15);
  payload[0] = methodId & 0xff;
  payload[1] = deviceId & 0xff;
  Buffer.from(acc, 'utf8').copy(payload, 2);
  return buildFrame(0x01, 0x02, 1, CMD_SLAVE_UNLOCK, payload);
}

function selfTest() {
  const cases = [
    { acc: '2', mid: 1, label: '从机密码开锁' },
    { acc: 'temporary ac', mid: 5, label: '从机临时密码(12B截断)' },
    { acc: '1001', mid: 2, label: '从机 NFC' },
  ];
  const out = [];
  for (const c of cases) {
    const frame = buildSlaveUnlockNotify(c.acc, c.mid, 2);
    const plen = frame[6] | (frame[7] << 8);
    const pl = frame.slice(HDR, HDR + plen);
    const parsed = parseUnlockNotify(pl);
    const crcGot = frame[HDR + plen] | (frame[HDR + plen + 1] << 8);
    const crcExp = crc16Modbus(frame, 2, HDR - 2 + plen);
    out.push({
      label: c.label,
      ok: parsed && parsed.method_id === c.mid && parsed.account === c.acc && crcGot === crcExp,
      frameHex: frame.toString('hex'),
      parsed,
    });
  }
  return out;
}

module.exports = {
  buildFrame,
  buildSlaveUnlockNotify,
  parseUnlockNotify,
  selfTest,
  CMD_SLAVE_UNLOCK,
};

if (require.main === module) {
  const r = selfTest();
  console.log('\n=== RS485 帧自测 ===\n');
  for (const row of r) {
    console.log(`${row.ok ? 'PASS' : 'FAIL'} ${row.label}`);
    console.log(`  parsed=${JSON.stringify(row.parsed)}`);
    console.log(`  hex=${row.frameHex}\n`);
  }
  process.exit(r.every((x) => x.ok) ? 0 : 1);
}
