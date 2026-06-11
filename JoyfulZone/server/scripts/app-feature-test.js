'use strict';

/**
 * 逐项测试 App 依赖的后端 API（模拟 shared.js 各功能）
 * 用法: node scripts/app-feature-test.js [baseUrl]
 */
require('dotenv').config({ path: require('path').join(__dirname, '..', '.env') });

const BASE = process.argv[2] || 'http://127.0.0.1:3000';
const DEVICE = process.env.ALIYUN_LOCK_DEVICE_NAME || '1111';
const APP_ACCOUNT = '1001';

const results = [];

async function req(method, path, body, timeoutMs = 15000) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const opts = { method, signal: ctrl.signal, headers: {} };
    if (body != null) {
      opts.headers['Content-Type'] = 'application/json';
      opts.body = JSON.stringify(body);
    }
    const r = await fetch(`${BASE}${path}`, opts);
    const text = await r.text();
    let json;
    try {
      json = JSON.parse(text);
    } catch {
      json = { raw: text.slice(0, 300) };
    }
    return { status: r.status, json };
  } finally {
    clearTimeout(timer);
  }
}

function record(name, ok, detail) {
  results.push({ name, ok, detail });
  const mark = ok ? 'PASS' : 'FAIL';
  console.log(`[${mark}] ${name}${detail ? ` — ${detail}` : ''}`);
}

async function main() {
  console.log(`\n=== JoyfulZone App 功能 API 测试 ===`);
  console.log(`BASE=${BASE} device=${DEVICE}\n`);

  // 1. 健康检查
  try {
    const h = await req('GET', '/health');
    record(
      '1.health 后端健康',
      h.status === 200 && h.json.ok && h.json.mqttConnected,
      `mqtt=${h.json.mqttConnected} presence=${h.json.lockPresence?.online} src=${h.json.lockPresence?.source}`
    );
  } catch (e) {
    record('1.health 后端健康', false, e.message);
    console.log('\n后端未启动，请先 npm start');
    process.exit(1);
  }

  // 2. 绑定状态 + 在线
  let bound = false;
  let lockOnline = false;
  try {
    const b = await req('GET', `/devices/bind?deviceName=${DEVICE}`);
    bound = !!b.json.bound;
    lockOnline = !!b.json.lockOnline;
    record(
      '2.devices/bind 绑定与在线',
      b.json.ok,
      `bound=${bound} online=${lockOnline} source=${b.json.lockPresenceSource} offlineMs=${b.json.lockOfflineMs}`
    );
  } catch (e) {
    record('2.devices/bind 绑定与在线', false, e.message);
  }

  // 3. 设备状态
  try {
    const s = await req('GET', '/devices/status');
    record('3.devices/status MQTT桥', s.json.ok && s.json.serverMqttConnected, `mqtt=${s.json.serverMqttConnected}`);
  } catch (e) {
    record('3.devices/status MQTT桥', false, e.message);
  }

  // 4. 用户列表同步
  try {
    const u = await req('GET', `/users?deviceName=${DEVICE}&refresh=1`, null, 20000);
    record(
      '4.users 用户列表同步',
      u.status === 200 && u.json.ok && Array.isArray(u.json.users),
      `total=${u.json.total} synced=${u.json.synced}`
    );
    if (u.json.users?.length) {
      const admins = u.json.users.filter((x) => x.isAdmin);
      console.log(`       管理员: ${admins.map((a) => a.account).join(',') || '无'}`);
    }
  } catch (e) {
    record('4.users 用户列表同步', false, e.message);
  }

  // 5. 开锁记录
  try {
    const r = await req('GET', `/records?deviceName=${DEVICE}&months=1`);
    record('5.records 开锁记录', r.json.ok, `total=${r.json.total ?? '?'}`);
  } catch (e) {
    record('5.records 开锁记录', false, e.message);
  }

  // 6. 今日开锁统计
  try {
    const t = await req('GET', `/records/stats/today?deviceName=${DEVICE}`);
    record('6.records/today 今日统计', t.json.ok, `count=${t.json.count}`);
  } catch (e) {
    record('6.records/today 今日统计', false, e.message);
  }

  // 7. 通知中心
  try {
    const n = await req('GET', `/notifications?deviceName=${DEVICE}&months=1`);
    record('7.notifications 通知', n.json.ok, `total=${n.json.total ?? '?'}`);
  } catch (e) {
    record('7.notifications 通知', false, e.message);
  }

  // 8. 临时密码列表
  try {
    const tp = await req('GET', `/temp-passwords?deviceName=${DEVICE}`);
    record('8.temp-passwords 列表', tp.json.ok, `active=${tp.json.items?.length ?? 0}`);
  } catch (e) {
    record('8.temp-passwords 列表', false, e.message);
  }

  // 9. 远程开锁（需已绑定 + 在线）
  try {
    const fresh = await req('GET', `/devices/bind?deviceName=${DEVICE}`);
    if (fresh.json.ok) {
      lockOnline = !!fresh.json.lockOnline;
      console.log(
        `       刷新在线: online=${lockOnline} source=${fresh.json.lockPresenceSource} offlineMs=${fresh.json.lockOfflineMs}`
      );
    }
  } catch {
    /* keep prior lockOnline */
  }

  if (!bound) {
    record('9.unlock 远程开锁', false, '跳过：App 未绑定');
  } else if (!lockOnline) {
    record('9.unlock 远程开锁', false, '跳过：门锁离线');
  } else {
    try {
      const ul = await req('POST', '/devices/unlock', { appAccount: APP_ACCOUNT, deviceName: DEVICE }, 15000);
      record(
        '9.unlock 远程开锁',
        ul.json.ok === true,
        ul.json.ok ? 'ack ok' : `${ul.json.error}: ${ul.json.message || ''}`
      );
    } catch (e) {
      record('9.unlock 远程开锁', false, e.message);
    }
  }

  // 10. 临时密码生成（需绑定+在线）
  let tempId = null;
  if (!bound) {
    record('10.temp-password 生成下发', false, '跳过：未绑定');
  } else if (!lockOnline) {
    record('10.temp-password 生成下发', false, '跳过：门锁离线');
  } else {
    try {
      const cr = await req(
        'POST',
        '/temp-passwords',
        { deviceName: DEVICE, reservedAccounts: ['1', '2', '1001'] },
        15000
      );
      const ok = cr.json.ok === true && cr.json.tempPassword?.account;
      if (ok) {
        tempId = cr.json.tempPassword.id;
        console.log(
          `       临时账号=${cr.json.tempPassword.account} 密码=${cr.json.tempPassword.password} (请在室外屏测试)`
        );
      }
      record(
        '10.temp-password 生成下发',
        ok,
        ok ? '门锁 ack' : `${cr.json.error}: ${cr.json.message || ''}`
      );
    } catch (e) {
      record('10.temp-password 生成下发', false, e.message);
    }
  }

  // 11. 临时密码作废
  if (tempId) {
    try {
      const del = await req('DELETE', `/temp-passwords/${tempId}`);
      record('11.temp-password 作废', del.json.ok, '');
    } catch (e) {
      record('11.temp-password 作废', false, e.message);
    }
  } else {
    record('11.temp-password 作废', false, '跳过：未生成');
  }

  // 12. 配对码绑定（用假码，预期拒绝 — 验证下行通路）
  try {
    const bindTry = await req(
      'POST',
      '/devices/bind',
      { pairCode: '000000', appAccount: APP_ACCOUNT, deviceName: DEVICE },
      12000
    );
    const expectReject = bindTry.json.error === 'BIND_REJECTED' || bindTry.json.error === 'BIND_ACK_TIMEOUT';
    record(
      '12.bind 配对下行(假码)',
      expectReject,
      bindTry.json.error || bindTry.json.message || `status=${bindTry.status}`
    );
  } catch (e) {
    record('12.bind 配对下行(假码)', false, e.message);
  }

  // 13. 在线探测速度（连续 3 次 bind，看 source）
  try {
    const t0 = Date.now();
    const samples = [];
    for (let i = 0; i < 3; i += 1) {
      const p = await req('GET', `/devices/bind?deviceName=${DEVICE}`);
      samples.push({
        online: p.json.lockOnline,
        source: p.json.lockPresenceSource,
        ms: Date.now() - t0,
      });
      await new Promise((r) => setTimeout(r, 1200));
    }
    record(
      '13.presence 在线探测采样',
      true,
      samples.map((s) => `${s.ms}ms:${s.online}/${s.source}`).join(' | ')
    );
  } catch (e) {
    record('13.presence 在线探测采样', false, e.message);
  }

  const pass = results.filter((r) => r.ok).length;
  const fail = results.filter((r) => !r.ok).length;
  console.log(`\n=== 汇总: ${pass} 通过, ${fail} 失败 / 共 ${results.length} 项 ===\n`);
  process.exit(fail > 0 ? 1 : 0);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
