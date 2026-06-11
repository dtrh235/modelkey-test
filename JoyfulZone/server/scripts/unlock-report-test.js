'use strict';

/**
 * 模拟各开锁方式的上报路径（App 桥接 vs 阿里云物模型）
 * - 远程开锁：POST /devices/unlock → 门锁 report_unlock_record_ex(phone)
 * - 其他方式只能由门锁本地触发，此处列出 method_code 对照并测远程
 */
require('dotenv').config({ path: require('path').join(__dirname, '..', '.env') });

const BASE = process.argv[2] || 'http://127.0.0.1:3000';
const DEVICE = process.env.ALIYUN_LOCK_DEVICE_NAME || '1111';

const METHODS = [
  { name: '密码', code: 1, method: 'password', device: 'Host/Client 本地' },
  { name: 'NFC', code: 2, method: 'nfc', device: 'Host/Client 本地' },
  { name: '指纹', code: 3, method: 'fingerprint', device: 'Host/Client 本地' },
  { name: '手机远程', code: 4, method: 'remote/phone', device: 'App POST /devices/unlock' },
  { name: '临时密码', code: 5, method: 'temporary-password', device: 'Client/Host 本地' },
];

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
    return { status: r.status, json: await r.json().catch(() => ({})) };
  } finally {
    clearTimeout(t);
  }
}

async function main() {
  console.log('\n=== 开锁上报路径对照 ===\n');
  console.log('每次开锁门锁会发两条（若 MQTT 在线）：');
  console.log('  A) terminal/push  unlock_record  → 后端 server/push → App 记录/通知');
  console.log('  B) property/post  thing.event    → 阿里云物模型历史（控制台看到的）\n');
  console.log('| 方式 | method_code | 触发 |');
  console.log('|------|-------------|------|');
  for (const m of METHODS) {
    console.log(`| ${m.name} | ${m.code} | ${m.device} |`);
  }
  console.log('');

  const bind = await req('GET', `/devices/bind?deviceName=${DEVICE}`);
  if (!bind.json.bound || !bind.json.lockOnline) {
    console.log(`门锁不可用: bound=${bind.json.bound} online=${bind.json.lockOnline}`);
    console.log('请先让门锁 MQTT 连上（keepalive 须 30~1200s，勿用 15s）');
    process.exit(1);
  }

  const before = await req('GET', `/records/stats/today?deviceName=${DEVICE}`);
  const t0 = Date.now();
  console.log('--- 模拟 App 远程开锁 (method_code=4) ---');
  const ul = await req('POST', '/devices/unlock', { appAccount: '1001', deviceName: DEVICE });
  console.log(`unlock: ok=${ul.json.ok} error=${ul.json.error || '-'}`);

  await new Promise((r) => setTimeout(r, 8000));
  const after = await req('GET', `/records/stats/today?deviceName=${DEVICE}`);
  const recs = await req('GET', `/records?deviceName=${DEVICE}&months=0.01`);

  console.log(`今日开锁: ${before.json.count} → ${after.json.count}`);
  const recent = (recs.json.records || recs.json.items || []).slice(0, 3);
  if (recent.length) {
    console.log('最近记录样本:');
    for (const r of recent) {
      console.log(`  method=${r.unlock_method ?? r.method} acc=${r.unlock_account ?? r.account} time=${r.unlock_time ?? r.time}`);
    }
  }

  console.log('\n请在门锁串口确认单次开锁仅有：');
  console.log('  [UNLOCK] pub ok 一次（勿连刷 pub fail）');
  console.log('  且无多条 property/post 重试');
  console.log('阿里云控制台 1 小时内应只增加 1 条（非每 4s 一条）\n');
}

main().catch((e) => {
  console.error(e.message);
  process.exit(1);
});
