'use strict';
/**
 * 用户增删改 API 测试（需门锁在线）
 * node scripts/user-ops-test.js [baseUrl]
 */
require('dotenv').config({ path: require('path').join(__dirname, '..', '.env') });

const BASE = process.argv[2] || 'http://127.0.0.1:3000';
const DEVICE = process.env.ALIYUN_LOCK_DEVICE_NAME || '1111';

async function req(method, path, body, timeoutMs = 20000) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const opts = { method, signal: ctrl.signal, headers: {} };
    if (body != null) {
      opts.headers['Content-Type'] = 'application/json';
      opts.body = JSON.stringify(body);
    }
    const r = await fetch(`${BASE}${path}`, opts);
    const json = await r.json().catch(() => ({}));
    return { status: r.status, json };
  } finally {
    clearTimeout(timer);
  }
}

async function main() {
  console.log(`\n=== 用户管理 API 测试 BASE=${BASE} ===\n`);

  const bind = await req('GET', `/devices/bind?deviceName=${DEVICE}`);
  console.log(
    `在线: ${bind.json.lockOnline} source=${bind.json.lockPresenceSource} usersRev=${bind.json.lockUsersUpdatedAt}`
  );
  if (!bind.json.lockOnline) {
    console.log('门锁离线，跳过增删测试');
    process.exit(2);
  }

  const testAcc = `9${String(Date.now()).slice(-3)}`;
  console.log(`\n1. 添加用户 ${testAcc}...`);
  const add = await req('POST', '/users', {
    deviceName: DEVICE,
    account: testAcc,
    password: '123456',
    isAdmin: false,
  });
  console.log(`   status=${add.status} ok=${add.json.ok} err=${add.json.error || ''} msg=${add.json.message || ''}`);

  const list1 = await req('GET', `/users?deviceName=${DEVICE}`);
  const found = (list1.json.users || []).some((u) => u.acc === testAcc);
  console.log(`   列表含 ${testAcc}: ${found}`);

  console.log(`\n2. 删除用户 ${testAcc}...`);
  const del = await req('DELETE', `/users/${testAcc}?deviceName=${DEVICE}`);
  console.log(`   status=${del.status} ok=${del.json.ok} err=${del.json.error || ''} msg=${del.json.message || ''}`);

  const list2 = await req('GET', `/users?deviceName=${DEVICE}&refresh=1`, null, 25000);
  const gone = !(list2.json.users || []).some((u) => u.acc === testAcc);
  console.log(`   门锁列表已无 ${testAcc}: ${gone}`);

  const bind2 = await req('GET', `/devices/bind?deviceName=${DEVICE}`);
  console.log(`\nusersRev 后: ${bind2.json.lockUsersUpdatedAt}`);

  process.exit(add.json.ok && del.json.ok && found && gone ? 0 : 1);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
