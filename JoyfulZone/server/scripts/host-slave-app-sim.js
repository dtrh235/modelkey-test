'use strict';

/**
 * 从机开锁全链路模拟（RS485 → 主机 MQTT → App / 阿里云物模型）
 *
 * 约定（与固件一致）：
 * - 从机 RS485 0x10：device_id 恒为 2（侧门从机）
 * - 临时密码：unlock_account = "temporary account"，method = 5
 * - 人员账号来自主机用户表，从机只负责开锁上报
 *
 * 用法: node scripts/host-slave-app-sim.js [baseUrl]
 */
require('dotenv').config({ path: require('path').join(__dirname, '..', '.env') });

const { buildSlaveUnlockNotify, parseUnlockNotify } = require('./rs485-frame-sim');
const recordStore = require('../src/services/recordStore');
const aliyunOpenApi = require('../src/services/aliyunOpenApi');

const BASE = process.argv[2] || 'http://127.0.0.1:3000';
const DEVICE = process.env.ALIYUN_LOCK_DEVICE_NAME || '1111';
const SLAVE_DEVICE = 2;

/** 从机四种开锁（账号为 RS485 payload 中的 acc 字段） */
const SLAVE_CASES = [
  { label: '从机密码', rs485Acc: '1', method: 1, expectAccount: '1' },
  { label: '从机NFC', rs485Acc: '1001', method: 2, expectAccount: '1001' },
  { label: '从机指纹', rs485Acc: '2', method: 3, expectAccount: '2' },
  {
    label: '从机临时密码',
    rs485Acc: 'temporary account',
    rs485WireAcc: 'temporary ac',
    method: 5,
    expectAccount: 'temporary account',
  },
];

const results = [];

function record(name, ok, detail) {
  results.push({ name, ok, detail });
  console.log(`[${ok ? 'PASS' : 'FAIL'}] ${name}${detail ? ` — ${detail}` : ''}`);
}

async function req(method, path, body, timeoutMs = 15000) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const r = await fetch(`${BASE}${path}`, {
      method,
      headers: body ? { 'Content-Type': 'application/json' } : {},
      body: body ? JSON.stringify(body) : undefined,
      signal: ctrl.signal,
    });
    const json = await r.json().catch(() => ({}));
    return { status: r.status, json };
  } finally {
    clearTimeout(t);
  }
}

/** 模拟 Host app_rs485_slave_unlock_forward_cloud + cloud_ota_service 上报字段 */
function simulateHostForwardFromRs485(rs485Acc, methodId, tsOffsetMs = 0) {
  const deviceId = SLAVE_DEVICE;
  let pubAcc;
  if (methodId === 5) {
    pubAcc = 'temporary account';
  } else {
    pubAcc = rs485Acc;
  }
  const unlockTime = recordStore.formatUnlockTimeText(Date.now() + tsOffsetMs);
  const bridge = {
    cmd: 'unlock_record',
    unlock_account: pubAcc,
    unlock_method: methodId,
    unlock_device: deviceId,
    unlock_time: unlockTime,
    id: String(Date.now() + tsOffsetMs),
  };
  const property = {
    method: 'thing.event.property.post',
    id: bridge.id,
    version: '1.0',
    params: {
      unlock_account: pubAcc,
      unlock_method: methodId,
      unlock_device: deviceId,
      unlock_time: unlockTime,
    },
  };
  const rec = recordStore.ingest(
    DEVICE,
    {
      unlock_account: pubAcc,
      unlock_method: methodId,
      unlock_device: deviceId,
      unlock_time: unlockTime,
    },
    { source: 'rs485-sim-slave', ts: Date.now() + tsOffsetMs }
  );
  return { bridge, property, rec, pubAcc, deviceId };
}

function validatePropertyPayload(prop, expectAccount, method) {
  const p = prop.params;
  return (
    p.unlock_account === expectAccount &&
    p.unlock_method === method &&
    p.unlock_device === SLAVE_DEVICE &&
    /^\d{4}\.\d{2}\.\d{2} \d{2}:\d{2}$/.test(p.unlock_time)
  );
}

async function queryAliyunSlaveRecent() {
  if (!aliyunOpenApi.fetchPropertyHistory) {
    return { ok: false, reason: 'openapi_not_configured' };
  }
  try {
    const end = Date.now();
    const start = end - 7 * 86400000;
    const [accounts, methods, devices] = await Promise.all([
      aliyunOpenApi.fetchPropertyHistory('unlock_account', start, end),
      aliyunOpenApi.fetchPropertyHistory('unlock_method', start, end),
      aliyunOpenApi.fetchPropertyHistory('unlock_device', start, end),
    ]);
    const slavePts = [];
    for (const m of methods) {
      if (m.value !== '1' && m.value !== '2' && m.value !== '3' && m.value !== '5') {
        continue;
      }
      const dev = devices.find((d) => Math.abs(d.timeMs - m.timeMs) < 800);
      if (!dev || dev.value !== '2') {
        continue;
      }
      const acc = accounts.find((a) => Math.abs(a.timeMs - m.timeMs) < 800);
      slavePts.push({
        timeMs: m.timeMs,
        method: Number(m.value),
        account: acc ? acc.value : '?',
        device: 2,
      });
    }
    return { ok: true, slaveDevice2Count: slavePts.length, samples: slavePts.slice(-8) };
  } catch (e) {
    return { ok: false, reason: e.message };
  }
}

async function main() {
  console.log('\n=== 从机开锁 → 主机 → App / 阿里云 模拟 ===');
  console.log(`BASE=${BASE} device=${DEVICE} unlock_device=${SLAVE_DEVICE}\n`);

  // 1. RS485 帧 + 主机转发字段
  for (const c of SLAVE_CASES) {
    const frame = buildSlaveUnlockNotify(c.rs485Acc, c.method, SLAVE_DEVICE);
    const plen = frame[6] | (frame[7] << 8);
    const parsed = parseUnlockNotify(frame.slice(8, 8 + plen));
    const wireAcc = c.rs485WireAcc || c.rs485Acc;
    const frameOk =
      parsed &&
      parsed.method_id === c.method &&
      parsed.device_id === SLAVE_DEVICE &&
      parsed.account === wireAcc;
    record(`1.RS485 ${c.label}`, frameOk, JSON.stringify(parsed));

    const fwd = simulateHostForwardFromRs485(c.rs485Acc, c.method, c.method * 37);
    const bridgeOk =
      fwd.bridge.unlock_account === c.expectAccount &&
      fwd.bridge.unlock_method === c.method &&
      fwd.bridge.unlock_device === SLAVE_DEVICE;
    const propOk = validatePropertyPayload(fwd.property, c.expectAccount, c.method);
    record(
      `2.主机桥接 ${c.label}`,
      bridgeOk && !!fwd.rec,
      `acc=${fwd.pubAcc} dev=${fwd.deviceId}`
    );
    record(`2.物模型 ${c.label}`, propOk, `account=${fwd.property.params.unlock_account}`);
  }

  // 3. App GET /records 能否读到（用 refresh 合并库内新写入）
  try {
    const api = await req('GET', `/records?deviceName=${DEVICE}&months=0.01&refresh=0`);
    const list = api.json.records || [];
    const simRows = list.filter((r) => r.source === 'rs485-sim-slave').slice(0, 8);
    for (const c of SLAVE_CASES) {
      const hit = simRows.find(
        (r) =>
          r.unlockMethod === c.method &&
          r.unlockAccount === c.expectAccount &&
          r.unlockDevice === SLAVE_DEVICE
      );
      record(
        `3.App记录 ${c.label}`,
        !!hit,
        hit ? `${hit.unlockTime} dev=${hit.unlockDevice}` : '未在 API 返回中找到'
      );
    }
  } catch (e) {
    record('3.App GET /records', false, e.message);
  }

  // 4. 阿里云历史：device=2 的物模型点（真机上报；模拟只验证 OpenAPI 可读）
  const ali = await queryAliyunSlaveRecent();
  if (!ali.ok) {
    record('4.阿里云 OpenAPI', false, ali.reason || '未配置 AccessKey');
  } else {
    record('4.阿里云 OpenAPI', true, '可查询物模型历史');
    console.log(
      `   7天内 unlock_device=2 共 ${ali.slaveDevice2Count} 点（须真机从机开锁后才会增加）`
    );
    if (ali.samples.length) {
      console.log('   最近 sample:', ali.samples.map((s) => `m${s.method}/${s.account}`).join(', '));
    } else {
      console.log('   尚无 device=2 历史点：请烧录 Host+Client 后从机实机开锁再查阿里云控制台。');
    }
  }

  const pass = results.filter((r) => r.ok).length;
  const fail = results.length - pass;
  console.log(`\n=== 汇总: ${pass} 通过, ${fail} 失败 / 共 ${results.length} 项 ===\n`);
  process.exit(fail > 0 ? 1 : 0);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
