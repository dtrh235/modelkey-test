/**
 * JoyfulZone — modelkey lock UI preview v0.3
 * 纸质文艺 · 锁孔 · 触屏手势 · 全屏开锁涟漪
 */
(function () {
  'use strict';

  const ICONS = {
    user: '<svg viewBox="0 0 24 24"><circle cx="12" cy="8" r="3.5"/><path d="M5 20c0-4 3-6 7-6s7 2 7 6"/></svg>',
    trash: '<svg viewBox="0 0 24 24"><path d="M6 7h12M9 7V5h6v2M8 7l1 12h6l1-12"/></svg>',
    search: '<svg viewBox="0 0 24 24"><circle cx="10" cy="10" r="5.5"/><path d="M15 15l5 5"/></svg>',
    list: '<svg viewBox="0 0 24 24"><path d="M6 7h12M6 12h12M6 17h8"/></svg>',
    wifi: '<svg viewBox="0 0 24 24"><path d="M4 10c5-5 11-5 16 0M7.5 13.5c3-3 6.5-3 9.5 0M11 17h2a1 1 0 1 0 0-2h-2a1 1 0 1 0 0 2z"/></svg>',
    link: '<svg viewBox="0 0 24 24"><path d="M10 14a4 4 0 0 1 0-5.5l1-1a4 4 0 0 1 5.5 0M14 10a4 4 0 0 1 0 5.5l-1 1a4 4 0 0 1-5.5 0"/></svg>',
    fp: '<svg viewBox="0 0 24 24"><path d="M12 3c-2 3-4 5-4 9a4 4 0 0 0 8 0c0-2-1-3.5-2-5M9 14c0 2 1.5 4 3 4s3-2 3-4"/></svg>',
    nfc: '<svg viewBox="0 0 24 24"><path d="M7 12a5 5 0 0 1 10 0M9 12a3 3 0 0 1 6 0M12 12v3"/></svg>',
  };

  const SCREENS = {
    home:     { id: 'home',     title: '首页',         group: 'main' },
    unlock:   { id: 'unlock',   title: '用户开锁',     group: 'auth' },
    admin:    { id: 'admin',    title: '管理员登录',   group: 'auth' },
    menu:     { id: 'menu',     title: '账号管理',     group: 'admin' },
    addUser:  { id: 'addUser',  title: '增加用户',     group: 'admin' },
    delUser:  { id: 'delUser',  title: '删除用户',     group: 'admin' },
    search:   { id: 'search',   title: '查找用户',     group: 'admin' },
    editUser: { id: 'editUser', title: '更改用户',     group: 'admin' },
    userList: { id: 'userList', title: '用户列表',     group: 'admin' },
    wifi:     { id: 'wifi',     title: '无线网络',     group: 'admin' },
    pair:     { id: 'pair',     title: '设备配对',     group: 'admin' },
    nfcMgmt:  { id: 'nfcMgmt',  title: 'NFC 管理',     group: 'sub' },
    fpMgmt:   { id: 'fpMgmt',   title: '指纹管理',     group: 'sub' },
  };

  const MENU_ITEMS = [
    { icon: 'user',   label: '增加用户',       sub: '录入账号与凭证', screen: 'addUser' },
    { icon: 'trash',  label: '删除用户',       sub: '按学号移除',     screen: 'delUser' },
    { icon: 'search', label: '查找(更改)用户', sub: '搜索并编辑',     screen: 'search' },
    { icon: 'list',   label: '显示所有用户',   sub: '完整名单',       screen: 'userList' },
    { icon: 'wifi',   label: '设置无线网络',   sub: '网络与云端',     screen: 'wifi' },
    { icon: 'link',   label: '设备配对',       sub: 'JoyfulZone',     screen: 'pair' },
  ];

  const MOCK_USERS = [
    { acc: '20230101', role: 'admin', fp: true, nfc: true },
    { acc: '20230215', role: 'user',  fp: true, nfc: false },
    { acc: '20230308', role: 'user',  fp: false, nfc: true },
    { acc: '20230422', role: 'user',  fp: false, nfc: false },
  ];

  const WIFI_LIST = [
    { ssid: '办公室-5G', rssi: 4, connected: true },
    { ssid: '访客网络',  rssi: 3, connected: false },
    { ssid: '物联网桥接', rssi: 2, connected: false },
    { ssid: '家里WiFi',  rssi: 3, connected: false },
  ];

  const LIST_ROW_H = 52;
  const LIST_VISIBLE = 5;
  const WIFI_VISIBLE = 5;
  const ADD_SCROLL_STEP = 38;
  const ADD_SCROLL_MAX = 2;
  const TABLE_ROW_H = 36;

  const MOTION_HTML = {
    success: `<div class="motion-wrap">
      <svg viewBox="0 0 56 56" class="motion-svg" aria-hidden="true">
        <circle class="ms-stroke ms-circle" cx="28" cy="28" r="24"/>
        <path class="ms-stroke ms-check" d="M17 29 L25 37 L39 21"/>
      </svg>
    </div>`,
    fail: `<div class="motion-wrap">
      <svg viewBox="0 0 56 56" class="motion-svg fail" aria-hidden="true">
        <circle class="ms-stroke ms-circle" cx="28" cy="28" r="24"/>
        <path class="ms-stroke ms-x" d="M20 20 L36 36"/>
        <path class="ms-stroke ms-x" d="M36 20 L20 36"/>
      </svg>
    </div>`,
    clearOk: `<div class="motion-wrap">
      <svg viewBox="0 0 56 56" class="motion-svg clear-ok" aria-hidden="true">
        <circle class="ms-stroke ms-circle" cx="28" cy="28" r="24"/>
        <path class="ms-stroke ms-check" d="M17 29 L25 37 L39 21"/>
      </svg>
    </div>`,
    clearFail: `<div class="motion-wrap">
      <svg viewBox="0 0 56 56" class="motion-svg fail" aria-hidden="true">
        <circle class="ms-stroke ms-circle" cx="28" cy="28" r="24"/>
        <path class="ms-stroke ms-x" d="M20 20 L36 36"/>
        <path class="ms-stroke ms-x" d="M36 20 L20 36"/>
      </svg>
    </div>`,
  };

  const state = {
    screen: 'home',
    prevStack: [],
    menuIndex: 0,
    menuScroll: 0,
    tableScroll: 0,
    editScroll: 0,
    wifiScroll: 0,
    homeSelected: null,
    homeSensing: false,
    addField: 0,
    unlockField: 0,
    unlockAcc: '',
    unlockPwd: '',
    unlockErrors: 0,
    lockoutUntil: 0,
    adminAcc: '',
    adminPwd: '',
    adminField: 0,
    searchAcc: '',
    delAcc: '',
    addAcc: '',
    addPwd: '',
    addAdmin: false,
    addScroll: 0,
    wifiIndex: 0,
    wifiScanning: false,
    wifiPwdOpen: false,
    wifiPwd: '',
    pairCode: '384729',
    mqttOnline: true,
    foundUser: null,
  };

  const $ = (s, c = document) => c.querySelector(s);
  const root = () => $('#screen-root');
  const modalEl = () => $('#modal-overlay');
  const toastEl = () => $('#toast');

  function pad(n) { return String(n).padStart(2, '0'); }

  function updateClock() {
    const now = new Date();
    const t = `${pad(now.getHours())}:${pad(now.getMinutes())}`;
    const d = `${now.getFullYear()} · ${pad(now.getMonth() + 1)} · ${pad(now.getDate())}`;
    document.querySelectorAll('[data-clock]').forEach(el => { el.textContent = t; });
    document.querySelectorAll('[data-date]').forEach(el => { el.textContent = d; });
  }

  function signalHtml(level) {
    return [1, 2, 3, 4].map(i =>
      `<i class="${i <= level ? 'on' : ''}"></i>`
    ).join('');
  }

  function topbar() {
    return `
      <div class="topbar">
        <span class="topbar-time" data-clock>--:--</span>
        <div class="topbar-meta">
          <div class="signal-dots">${signalHtml(4)}</div>
          <div class="cloud-pill ${state.mqttOnline ? '' : 'off'}">
            <span class="cloud-label">${state.mqttOnline ? '云端' : '离线'}</span>
          </div>
        </div>
      </div>`;
  }

  function footer(ok = '确认', esc = true, cls = 'fill') {
    return `
      <div class="footer-dock">
        ${esc ? '<button class="dock-btn ghost sm" data-action="esc">返回</button>' : ''}
        <button class="dock-btn ${cls}" data-action="ok">${ok}</button>
      </div>`;
  }

  function footerBack() {
    return `
      <div class="footer-dock">
        <button class="dock-btn ghost" data-action="esc">返回</button>
      </div>`;
  }

  function homeFooter() {
    const m = state.homeSelected === 'menu' ? 'selected' : '';
    const u = state.homeSelected === 'unlock' ? 'selected' : '';
    return `
      <div class="footer-dock">
        <button class="dock-btn ghost ${m}" data-action="go-menu">菜单</button>
        <button class="dock-btn fill ${u}" data-action="go-unlock">开锁</button>
      </div>`;
  }

  function lockSvg() {
    const cls = state.homeSensing ? 'sensing' : '';
    return `
      <div class="lock-sculpture ${cls}">
        <svg class="lock-ring-svg" viewBox="0 0 96 96" aria-hidden="true">
          <circle class="lock-arc" cx="48" cy="48" r="38"/>
          <circle class="lock-core" cx="48" cy="52" r="28"/>
          <g class="keyhole-glyph-wrap" transform="translate(48 51)">
            <circle class="keyhole-glyph" cx="0" cy="-7" r="6.5"/>
            <path class="keyhole-glyph" d="M-2.4 0.6 L-6.5 12.2 H6.5 L2.4 0.6 Z"/>
          </g>
        </svg>
      </div>`;
  }

  function virtualList(items, selectedIdx, scrollOffset, rowH, visible, renderRow) {
    const maxScroll = Math.max(0, items.length - visible);
    const offset = Math.min(scrollOffset, maxScroll);
    const trackY = -offset * rowH;
    return `
      <div class="list-viewport no-scroll list-viewport-grow">
        <div class="list-track" style="transform:translateY(${trackY}px)">
          ${items.map((item, i) => renderRow(item, i, i === selectedIdx)).join('')}
        </div>
      </div>`;
  }

  function buildHome() {
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="home-main">
          <div class="home-clock" data-clock>--:--</div>
          <div class="home-date" data-date>----</div>
          ${lockSvg()}
          <div class="sense-row">
            <div class="sense-hint"><span class="sense-dot"></span>请刷卡</div>
            <div class="sense-hint"><span class="sense-dot"></span>请按指纹</div>
          </div>
        </div>
        ${homeFooter()}
      </div></div>`;
  }

  function buildUnlock() {
    const locked = state.lockoutUntil > Date.now();
    const rem = locked ? Math.ceil((state.lockoutUntil - Date.now()) / 1000) : 0;
    const f0 = locked ? 'disabled' : (state.unlockField === 0 ? 'focus' : '');
    const f1 = locked ? 'disabled' : (state.unlockField === 1 ? 'focus' : '');
    const c0 = locked ? '' : '<span class="caret"></span>';
    const c1 = locked ? '' : '<span class="caret"></span>';
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head"><h3>验证身份</h3><p>学号 / 工号与密码</p></div>
        <div class="form-zone ${locked ? 'is-locked' : ''}">
          <div class="field-block">
            <div class="field-label">账号</div>
            <div class="field-box ${f0}" data-field="0">${state.unlockAcc}${c0}</div>
          </div>
          <div class="field-block">
            <div class="field-label">密码</div>
            <div class="field-box ${f1}" data-field="1">${'·'.repeat(state.unlockPwd.length)}${c1}</div>
          </div>
          <div id="unlock-err" class="hint-err" style="display:none">凭证不正确</div>
          ${locked ? `<div class="hint-lock"><strong>${rem}</strong>秒后可再次尝试</div>` : ''}
        </div>
        ${footer('开锁', true, locked ? 'ghost' : 'fill')}
      </div></div>`;
  }

  function buildAdmin() {
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head"><h3>管理入口</h3><p>需要管理员权限</p></div>
        <div class="form-zone">
          <div class="field-block">
            <div class="field-label">管理员账号</div>
            <div class="field-box ${state.adminField === 0 ? 'focus' : ''}" data-field="0">${state.adminAcc}<span class="caret"></span></div>
          </div>
          <div class="field-block">
            <div class="field-label">密码</div>
            <div class="field-box ${state.adminField === 1 ? 'focus' : ''}" data-field="1">${'·'.repeat(state.adminPwd.length)}<span class="caret"></span></div>
          </div>
          <div id="admin-err" class="hint-err" style="display:none">验证失败</div>
        </div>
        ${footer('进入')}
      </div></div>`;
  }

  function buildMenu() {
    const list = virtualList(
      MENU_ITEMS, state.menuIndex, state.menuScroll, LIST_ROW_H, LIST_VISIBLE,
      (item, i, sel) => `
        <div class="list-card ${sel ? 'sel' : ''}" data-menu="${i}">
          <div class="ico">${ICONS[item.icon]}</div>
          <div><div class="txt">${item.label}</div><div class="sub">${item.sub}</div></div>
          <span class="chev">›</span>
        </div>`
    );
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head page-head-slim"><h3>账号管理</h3></div>
        ${list}
        ${footer('进入')}
      </div></div>`;
  }

  function buildUserList() {
    const rowH = LIST_ROW_H;
    const maxScroll = Math.max(0, MOCK_USERS.length - LIST_VISIBLE);
    const off = Math.min(state.tableScroll, maxScroll);
    const rows = MOCK_USERS.map(u => `
      <div class="scroll-card">
        <div class="scroll-card-body">
          <div class="scroll-card-title">${u.acc}</div>
          <div class="scroll-card-sub">${u.role === 'admin' ? '管理员' : '用户'}</div>
        </div>
        <span class="tag-role tag-${u.role}">${u.role === 'admin' ? '管理员' : '用户'}</span>
      </div>`).join('');
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head"><h3>全部用户</h3><p>${MOCK_USERS.length} 位已注册</p></div>
        <div class="scroll-panel">
          <div class="list-viewport no-scroll list-viewport-grow" style="margin:0">
            <div class="list-track" style="transform:translateY(${-off * rowH}px)">${rows}</div>
          </div>
        </div>
        ${footerBack()}
      </div></div>`;
  }

  function buildSearch() {
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head"><h3>查找用户</h3><p>输入学号 / 工号</p></div>
        <div class="form-zone">
          <div class="field-block">
            <div class="field-label">学号 / 工号</div>
            <div class="field-box focus" data-field="0">${state.searchAcc}<span class="caret"></span></div>
          </div>
          <div id="search-err" class="hint-err" style="display:none">未找到该用户</div>
        </div>
        ${footer('搜索')}
      </div></div>`;
  }

  function buildEditUser() {
    const u = state.foundUser || MOCK_USERS[1];
    const rowH = LIST_ROW_H;
    const visible = 4;
    const items = [
      { k: '账号', v: u.acc, a: 'noop' },
      { k: '密码', v: '······', a: 'noop' },
      { k: '指纹', v: u.fp ? '已录入' : '未录入', a: 'go-fp' },
      { k: '身份', v: u.role === 'admin' ? '管理员' : '用户', a: 'noop' },
      { k: 'NFC 卡', v: u.nfc ? '已绑定' : '未绑定', a: 'go-nfc' },
    ];
    const maxScroll = Math.max(0, items.length - visible);
    const off = Math.min(state.editScroll, maxScroll);
    const cards = items.map(it => `
      <div class="detail-card">
        <div><div class="k">${it.k}</div><div class="v">${it.v}</div></div>
        <button class="chip-btn" data-action="${it.a}">更改</button>
      </div>`).join('');
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head page-head-slim"><h3>${u.acc}</h3><p>编辑用户资料</p></div>
        <div class="scroll-panel">
          <div class="list-viewport no-scroll list-viewport-grow" style="margin:0">
            <div class="list-track" style="transform:translateY(${-off * rowH}px)">${cards}</div>
          </div>
        </div>
        ${footerBack()}
      </div></div>`;
  }

  function buildAddUser() {
    const f0 = state.addField === 0 ? 'focus' : '';
    const f1 = state.addField === 1 ? 'focus' : '';
    const f2 = state.addField === 2 ? 'focus' : '';
    const trackY = -state.addScroll * ADD_SCROLL_STEP;
    return `
      <div class="screen-inner"><div class="screen-body screen-body-add">
        ${topbar()}
        <div class="page-head page-head-tight"><h3>新用户</h3></div>
        <div class="add-scroll-panel">
          <div class="add-viewport no-scroll">
            <div class="add-scroll-track" style="transform:translateY(${trackY}px)">
              <div class="field-block tight">
                <div class="field-label">学号 / 工号</div>
                <div class="field-box field-sm ${f0}" data-field="0">${state.addAcc}${state.addField === 0 ? '<span class="caret"></span>' : ''}</div>
              </div>
              <div class="field-block tight">
                <div class="field-label">密码</div>
                <div class="field-box field-sm ${f1}" data-field="1">${'·'.repeat(state.addPwd.length)}${state.addField === 1 ? '<span class="caret"></span>' : ''}</div>
              </div>
              <div class="field-block tight">
                <div class="field-label">身份</div>
                <div class="role-picker ${f2 ? 'focus' : ''}" data-field="2">
                  <button type="button" class="role-opt ${!state.addAdmin ? 'on' : ''}" data-action="role-user">用户</button>
                  <button type="button" class="role-opt ${state.addAdmin ? 'on' : ''}" data-action="role-admin">管理员</button>
                </div>
              </div>
              <div class="bio-block">
                <div class="bio-head">生物凭证</div>
                <div class="bio-row" data-action="enroll-fp">
                  <span class="bio-ico">${ICONS.fp}</span>
                  <span class="bio-label">指纹录入</span>
                  <span class="bio-action">录入</span>
                </div>
                <div class="bio-row" data-action="enroll-nfc">
                  <span class="bio-ico">${ICONS.nfc}</span>
                  <span class="bio-label">NFC 录入</span>
                  <span class="bio-action">录入</span>
                </div>
              </div>
            </div>
          </div>
        </div>
        ${footer('保存')}
      </div></div>`;
  }

  function buildDelUser() {
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head"><h3>删除用户</h3><p>此操作不可撤销</p></div>
        <div class="form-zone">
          <div class="field-block">
            <div class="field-label">学号 / 工号</div>
            <div class="field-box focus" data-field="0">${state.delAcc}<span class="caret"></span></div>
          </div>
        </div>
        ${footer('删除')}
      </div></div>`;
  }

  function buildWifi() {
    const conn = WIFI_LIST.find(w => w.connected);
    const maxScroll = Math.max(0, WIFI_LIST.length - WIFI_VISIBLE);
    const off = Math.min(state.wifiScroll, maxScroll);
    const items = WIFI_LIST.map((w, i) => `
      <div class="wifi-item ${i === state.wifiIndex ? 'sel' : ''}" data-wifi="${i}">
        <div class="signal-dots">${signalHtml(w.rssi)}</div>
        <div class="wifi-name">${w.ssid}</div>
        ${w.connected ? '<span class="wifi-tag">已连接</span>' : ''}
      </div>`).join('');
    return `
      <div class="screen-inner"><div class="screen-body screen-body-wifi">
        ${topbar()}
        <div class="page-head page-head-slim wifi-head" style="transform:translateY(var(--pull, 0px))">
          <h3>无线网络</h3>
        </div>
        <div class="wifi-banner compact">
          <div class="signal-dots">${signalHtml(conn ? 4 : 1)}</div>
          <div>
            <div class="state">${conn ? conn.ssid : '未连接'}</div>
            <div class="sub">${conn ? '云端通道就绪' : '请选择网络'}</div>
          </div>
        </div>
        <div class="scan-link ${state.wifiScanning ? 'scanning' : ''}" data-action="wifi-scan">
          ${state.wifiScanning ? '扫描中…' : '刷新列表'}
        </div>
        <div class="scroll-panel scroll-panel-wifi">
          <div class="list-viewport no-scroll list-viewport-grow" style="margin:0">
            <div class="list-track" style="transform:translateY(${-off * 46}px)">${items}</div>
          </div>
        </div>
        ${footer('连接')}
      </div></div>`;
  }

  function buildPair() {
    const digits = state.pairCode.split('').map(c => `<div class="pair-ch">${c}</div>`).join('');
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head"><h3>设备配对</h3><p>JoyfulZone 智慧门禁</p></div>
        <div class="pair-zone">
          <div class="pair-brand">JoyfulZone</div>
          <div class="pair-cloud ${state.mqttOnline ? 'ok' : 'wait'}">
            ${state.mqttOnline ? '云端在线' : '等待连接'}
          </div>
          <div class="pair-code-row">${digits}</div>
        </div>
        <div class="footer-dock">
          <button class="dock-btn ghost sm" data-action="esc">返回</button>
          <button class="dock-btn ghost" data-action="pair-regen" style="flex:1">重新生成</button>
        </div>
      </div></div>`;
  }

  function buildSubPage(title, sub, actions) {
    return `
      <div class="screen-inner"><div class="screen-body">
        ${topbar()}
        <div class="page-head"><h3>${title}</h3><p>${sub}</p></div>
        <div class="action-stack">${actions}</div>
        ${footerBack()}
      </div></div>`;
  }

  const BUILDERS = {
    home: buildHome,
    unlock: buildUnlock,
    admin: buildAdmin,
    menu: buildMenu,
    addUser: buildAddUser,
    delUser: buildDelUser,
    search: buildSearch,
    editUser: buildEditUser,
    userList: buildUserList,
    wifi: buildWifi,
    pair: buildPair,
    nfcMgmt: () => buildSubPage('NFC 卡', '更改或移除卡片',
      '<button class="dock-btn fill" data-action="nfc-change">更换卡片</button><button class="dock-btn danger" data-action="nfc-delete">移除 NFC</button>'),
    fpMgmt: () => buildSubPage('指纹', '更改或清除模板',
      '<button class="dock-btn fill" data-action="fp-change">重新录入</button><button class="dock-btn danger" data-action="fp-delete">清除指纹</button>'),
  };

  function navigate(to, push = true) {
    if (push && state.screen !== to) state.prevStack.push(state.screen);
    if (to === 'addUser') {
      state.addScroll = 0;
      state.addField = 0;
    }
    document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
    const next = $(`.screen[data-screen="${to}"]`);
    if (next) next.classList.add('active');
    state.screen = to;
    syncSidebar();
    updateInfo();
  }

  function goBack() {
    if (state.prevStack.length) navigate(state.prevStack.pop(), false);
    else navigate('home', false);
  }

  function handleBackSwipe() {
    if (state.wifiPwdOpen) { hideWifiPwdModal(); return; }
    handleKey('ESC');
  }

  function scrollSteps(steps) {
    if (!steps) return;
    if (state.screen === 'addUser') {
      const delta = steps > 0 ? 1 : -1;
      const n = Math.min(Math.abs(steps), 4);
      let next = state.addScroll;
      for (let i = 0; i < n; i++) {
        next = Math.max(0, Math.min(ADD_SCROLL_MAX, next + delta));
      }
      if (next !== state.addScroll) {
        state.addScroll = next;
        rerender();
      }
      return;
    }
    const dir = steps > 0 ? 'DOWN' : 'UP';
    const n = Math.min(Math.abs(steps), 6);
    for (let i = 0; i < n; i++) handleKey(dir);
  }

  function patchHomeOnly() {
    const home = $(`.screen[data-screen="home"]`);
    if (!home) { rerender(); return; }
    home.innerHTML = buildHome();
    updateClock();
    bindEvents();
  }

  function syncSidebar() {
    document.querySelectorAll('.nav-btn').forEach(b => {
      b.classList.toggle('active', b.dataset.screen === state.screen);
    });
  }

  function updateInfo() {
    const el = $('#screen-info-text');
    if (el) {
      const s = SCREENS[state.screen];
      el.innerHTML = `当前：<strong>${s?.title || state.screen}</strong> · 240×320 · v0.3`;
    }
  }

  function toast(msg) {
    const el = toastEl();
    if (!el) return;
    el.textContent = msg;
    el.classList.add('show');
    clearTimeout(el._t);
    el._t = setTimeout(() => el.classList.remove('show'), 2200);
  }

  function showAnimatedModal({ title, sub = '', type = 'success', duration = 2400 }) {
    const o = modalEl();
    if (!o) return;
    const motion = MOTION_HTML[type] || MOTION_HTML.success;
    const shake = (type === 'fail' || type === 'clearFail') ? ' shake' : '';
    o.innerHTML = `
      <div class="modal-sheet motion-sheet${shake}">
        ${motion}
        <div class="modal-title">${title}</div>
        <div class="modal-sub">${sub || ''}</div>
      </div>`;
    o.classList.add('show');
    o.onclick = null;
    requestAnimationFrame(() => {
      o.querySelector('.motion-sheet')?.classList.add('play');
    });
    clearTimeout(o._t);
    o._t = setTimeout(hideModal, duration);
  }

  function showModal(title, sub, type = 'ok') {
    if (type === 'scan') {
      const o = modalEl();
      if (!o) return;
      o.innerHTML = `
        <div class="modal-sheet motion-sheet play">
          <div class="modal-ring scan"></div>
          <div class="modal-title">${title}</div>
          <div class="modal-sub">${sub || ''}</div>
        </div>`;
      o.classList.add('show');
      clearTimeout(o._t);
      o._t = setTimeout(hideModal, 99999);
      return;
    }
    showAnimatedModal({ title, sub, type: 'success' });
  }

  function hideModal() {
    const o = modalEl();
    if (!o) return;
    o.classList.remove('show');
    o.onclick = null;
  }

  function showFpModal(doneTitle, success = true) {
    const o = modalEl();
    if (!o) return;
    o.innerHTML = `
      <div class="modal-sheet motion-sheet loading-sheet">
        <div class="fp-scanner"><div class="fp-glyph">${ICONS.fp}</div></div>
        <div class="modal-title">指纹录入中</div>
        <div class="modal-sub">请按压手指 3 次</div>
      </div>`;
    o.classList.add('show');
    o.onclick = null;
    requestAnimationFrame(() => o.querySelector('.motion-sheet')?.classList.add('play'));
    clearTimeout(o._t);
    o._t = setTimeout(() => {
      hideModal();
      setTimeout(() => showAnimatedModal({
        title: success ? (doneTitle || '录入完成') : '录入失败',
        sub: success ? '' : '请重试',
        type: success ? 'success' : 'fail',
      }), 160);
    }, 2800);
  }

  function showNfcModal(doneTitle, success = true) {
    const o = modalEl();
    if (!o) return;
    o.innerHTML = `
      <div class="modal-sheet motion-sheet loading-sheet">
        <div class="nfc-ripple-wrap">
          <div class="nfc-ripple"></div>
          <div class="nfc-ripple"></div>
          <div class="nfc-ripple"></div>
          <div class="nfc-glyph">${ICONS.nfc}</div>
        </div>
        <div class="modal-title">NFC 录入中</div>
        <div class="modal-sub">请将卡片靠近读卡区</div>
      </div>`;
    o.classList.add('show');
    o.onclick = null;
    requestAnimationFrame(() => o.querySelector('.motion-sheet')?.classList.add('play'));
    clearTimeout(o._t);
    o._t = setTimeout(() => {
      hideModal();
      setTimeout(() => showAnimatedModal({
        title: success ? (doneTitle || '绑定完成') : '录入失败',
        sub: success ? '' : '请重试',
        type: success ? 'success' : 'fail',
      }), 160);
    }, 2400);
  }

  function unlockOk() {
    const ds = $('.device-screen');
    if (!ds) return;
    const layer = document.createElement('div');
    layer.className = 'unlock-ripple-layer';
    layer.innerHTML = `
      <div class="ripple-ring ripple-ring-1"></div>
      <div class="ripple-ring ripple-ring-2"></div>
      <div class="ripple-ring ripple-ring-3"></div>
      <div class="unlock-ripple-text">
        <div class="u-title">门已打开</div>
        <div class="u-sub">欢迎</div>
      </div>`;
    ds.appendChild(layer);
    requestAnimationFrame(() => layer.classList.add('show'));
    setTimeout(() => {
      layer.classList.remove('show');
      setTimeout(() => layer.remove(), 700);
      state.homeSensing = false;
      navigate('home', false);
      patchHomeOnly();
    }, 2200);
  }

  function showWifiPwdModal() {
    const o = modalEl();
    if (!o) return;
    const w = WIFI_LIST[state.wifiIndex];
    if (w.connected) { toast('已连接'); return; }
    state.wifiPwdOpen = true;
    state.wifiPwd = '';
    o.innerHTML = `
      <div class="modal-sheet motion-sheet wifi-pwd-sheet">
        <div class="modal-title">连接网络</div>
        <div class="modal-sub">${w.ssid}</div>
        <div class="wifi-pwd-field">${''}<span class="caret"></span></div>
        <p class="wifi-pwd-hint">请用下方物理键输入密码</p>
        <div class="wifi-pwd-actions">
          <button class="dock-btn ghost sm" data-wifi-pwd="cancel">取消</button>
          <button class="dock-btn fill" data-wifi-pwd="ok">连接</button>
        </div>
      </div>`;
    o.classList.add('show');
    requestAnimationFrame(() => o.querySelector('.motion-sheet')?.classList.add('play'));
    o.onclick = (e) => {
      const b = e.target.closest('[data-wifi-pwd]');
      if (!b) return;
      if (b.dataset.wifiPwd === 'cancel') hideWifiPwdModal();
      else confirmWifiConnect();
    };
  }

  function hideWifiPwdModal() {
    state.wifiPwdOpen = false;
    state.wifiPwd = '';
    hideModal();
  }

  function updateWifiPwdDisplay() {
    const field = $('.wifi-pwd-field');
    if (field) field.innerHTML = `${'·'.repeat(state.wifiPwd.length)}<span class="caret"></span>`;
  }

  function confirmWifiConnect() {
    hideWifiPwdModal();
    WIFI_LIST.forEach(w => { w.connected = false; });
    WIFI_LIST[state.wifiIndex].connected = true;
    showAnimatedModal({ title: '连接成功', sub: WIFI_LIST[state.wifiIndex].ssid, type: 'success' });
    rerender();
  }

  function clampMenuScroll() {
    const max = Math.max(0, MENU_ITEMS.length - LIST_VISIBLE);
    if (state.menuIndex < state.menuScroll) state.menuScroll = state.menuIndex;
    if (state.menuIndex >= state.menuScroll + LIST_VISIBLE) state.menuScroll = state.menuIndex - LIST_VISIBLE + 1;
    state.menuScroll = Math.min(state.menuScroll, max);
  }

  function render() {
    const r = root();
    if (!r) return;
    const cur = state.screen;
    r.innerHTML = Object.keys(SCREENS).map(id => `
      <div class="screen ${id === cur ? 'active' : ''}" data-screen="${id}">
        ${BUILDERS[id]()}
      </div>`).join('');
    updateClock();
    bindEvents();
  }

  function rerender() {
    const s = state.screen;
    render();
    state.screen = s;
    $(`.screen[data-screen="${s}"]`)?.classList.add('active');
  }

  function focusField(scr, idx) {
    const n = +idx;
    if (scr === 'unlock') state.unlockField = n;
    else if (scr === 'admin') state.adminField = n;
    else if (scr === 'addUser') state.addField = Math.min(n, 2);
    rerender();
  }

  function handleKey(key) {
    if (state.wifiPwdOpen) {
      if (key === 'ESC') hideWifiPwdModal();
      else if (key === 'OK') confirmWifiConnect();
      else if (/^[a-z0-9]$/i.test(key) && state.wifiPwd.length < 32) {
        state.wifiPwd += key;
        updateWifiPwdDisplay();
      }
      return;
    }

    const scr = state.screen;

    if (scr === 'home') {
      if (key === 'LEFT') { state.homeSelected = 'menu'; patchHomeOnly(); }
      else if (key === 'RIGHT') { state.homeSelected = 'unlock'; patchHomeOnly(); }
      else if (key === 'OK') {
        if (state.homeSelected === 'unlock') navigate('unlock');
        else if (state.homeSelected === 'menu') navigate('admin');
        else toast('请选择入口');
      } else if (key === 'RANDOM') passiveUnlock();
    }
    else if (key === 'ESC') {
      if (scr === 'menu') { state.prevStack = []; navigate('home', false); }
      else goBack();
    }
    else if (scr === 'unlock') {
      if (state.lockoutUntil > Date.now()) {
        if (key === 'ESC') goBack();
        return;
      }
      if (key === 'UP') { state.unlockField = 0; rerender(); }
      else if (key === 'DOWN') { state.unlockField = 1; rerender(); }
      else if (key === 'OK') tryUnlock();
      else if (/^\d$/.test(key)) {
        if (state.unlockField === 0 && state.unlockAcc.length < 12) state.unlockAcc += key;
        else if (state.unlockField === 1 && state.unlockPwd.length < 10) state.unlockPwd += key;
        rerender();
      }
    }
    else if (scr === 'admin') {
      if (key === 'UP') { state.adminField = 0; rerender(); }
      else if (key === 'DOWN') { state.adminField = 1; rerender(); }
      else if (key === 'OK') tryAdmin();
      else if (/^[a-z0-9]$/i.test(key)) {
        if (state.adminField === 0 && state.adminAcc.length < 12) state.adminAcc += key;
        else if (state.adminField === 1 && state.adminPwd.length < 10) state.adminPwd += key;
        rerender();
      }
    }
    else if (scr === 'menu') {
      if (key === 'UP') {
        state.menuIndex = (state.menuIndex + MENU_ITEMS.length - 1) % MENU_ITEMS.length;
        clampMenuScroll();
        rerender();
      } else if (key === 'DOWN') {
        state.menuIndex = (state.menuIndex + 1) % MENU_ITEMS.length;
        clampMenuScroll();
        rerender();
      } else if (key === 'OK') navigate(MENU_ITEMS[state.menuIndex].screen);
    }
    else if (scr === 'userList') {
      const max = Math.max(0, MOCK_USERS.length - LIST_VISIBLE);
      if (key === 'UP') { state.tableScroll = Math.max(0, state.tableScroll - 1); rerender(); }
      else if (key === 'DOWN') { state.tableScroll = Math.min(max, state.tableScroll + 1); rerender(); }
    }
    else if (scr === 'editUser') {
      const max = Math.max(0, 5 - 4);
      if (key === 'UP') { state.editScroll = Math.max(0, state.editScroll - 1); rerender(); }
      else if (key === 'DOWN') { state.editScroll = Math.min(max, state.editScroll + 1); rerender(); }
    }
    else if (scr === 'wifi') {
      const max = Math.max(0, WIFI_LIST.length - WIFI_VISIBLE);
      if (key === 'UP') {
        state.wifiIndex = Math.max(0, state.wifiIndex - 1);
        if (state.wifiIndex < state.wifiScroll) state.wifiScroll = state.wifiIndex;
        rerender();
      } else if (key === 'DOWN') {
        state.wifiIndex = Math.min(WIFI_LIST.length - 1, state.wifiIndex + 1);
        if (state.wifiIndex >= state.wifiScroll + WIFI_VISIBLE) state.wifiScroll = state.wifiIndex - WIFI_VISIBLE + 1;
        state.wifiScroll = Math.min(max, state.wifiScroll);
        rerender();
      } else if (key === 'OK') showWifiPwdModal();
    }
    else if (scr === 'search' && key === 'OK') {
      const u = MOCK_USERS.find(x => x.acc === state.searchAcc);
      if (u) { state.foundUser = u; state.editScroll = 0; navigate('editUser'); }
      else {
        const e = $('#search-err');
        if (e) e.style.display = 'block';
        showAnimatedModal({ title: '未找到', sub: '用户不存在', type: 'fail' });
      }
    }
    else if (scr === 'addUser') {
      if (key === 'UP') {
        if (state.addScroll > 0 && state.addField === 0) state.addScroll--;
        else state.addField = (state.addField + 2) % 3;
        rerender();
      } else if (key === 'DOWN') {
        if (state.addScroll < ADD_SCROLL_MAX && state.addField === 2) state.addScroll++;
        else state.addField = (state.addField + 1) % 3;
        rerender();
      }
      else if (key === 'LEFT') { state.addAdmin = false; state.addField = 2; rerender(); }
      else if (key === 'RIGHT') { state.addAdmin = true; state.addField = 2; rerender(); }
      else if (key === 'OK') {
        const ok = state.addAcc.length >= 1 && state.addPwd.length >= 4;
        showAnimatedModal({
          title: ok ? '已保存' : '保存失败',
          sub: ok ? (state.addAcc || '新用户') : '请填写账号与密码',
          type: ok ? 'success' : 'fail',
        });
      }
      else if (/^[a-z0-9]$/i.test(key)) {
        if (state.addField === 0 && state.addAcc.length < 12) state.addAcc += key;
        else if (state.addField === 1 && state.addPwd.length < 10) state.addPwd += key;
        rerender();
      }
    }
    else if (scr === 'delUser' && key === 'OK') {
      const ok = state.delAcc.length > 0;
      showAnimatedModal({
        title: ok ? '已删除' : '删除失败',
        sub: ok ? state.delAcc : '请输入学号',
        type: ok ? 'clearOk' : 'fail',
      });
    }
  }

  function tryUnlock() {
    if (state.unlockAcc === '20230101' && state.unlockPwd === '1234') unlockOk();
    else {
      state.unlockErrors++;
      const e = $('#unlock-err');
      if (e) e.style.display = 'block';
      if (state.unlockErrors >= 3) {
        state.lockoutUntil = Date.now() + 12000;
        toast('已暂时锁定');
      }
      rerender();
    }
  }

  function tryAdmin() {
    if (state.adminAcc === 'admin' && state.adminPwd === 'admin') {
      navigate('menu');
      toast('验证通过');
    } else {
      const e = $('#admin-err');
      if (e) e.style.display = 'block';
      showAnimatedModal({ title: '验证失败', sub: '账号或密码错误', type: 'fail' });
    }
  }

  function passiveUnlock() {
    state.homeSensing = true;
    patchHomeOnly();
    toast('感应中…');
    setTimeout(unlockOk, 1100);
  }

  function regenPair() {
    state.pairCode = String(Math.floor(100000 + Math.random() * 900000));
    rerender();
    toast('已更新配对码');
  }

  function wifiScan() {
    if (state.wifiScanning) return;
    state.wifiScanning = true;
    rerender();
    setTimeout(() => {
      state.wifiScanning = false;
      WIFI_LIST.sort(() => Math.random() - 0.5);
      rerender();
      toast('列表已刷新');
    }, 1600);
  }

  function bindEvents() {
    const r = root();
    if (!r) return;
    r.onclick = (e) => {
      const t = e.target.closest('[data-action],[data-menu],[data-wifi]');
      if (!t) {
        const field = e.target.closest('[data-field]');
        if (field) focusField(state.screen, field.dataset.field);
        return;
      }
      const a = t.dataset.action;
      const mi = t.dataset.menu;
      const wi = t.dataset.wifi;
      if (mi !== undefined) { state.menuIndex = +mi; clampMenuScroll(); rerender(); return; }
      if (wi !== undefined) { state.wifiIndex = +wi; rerender(); return; }
      const acts = {
        esc: () => handleKey('ESC'),
        ok: () => handleKey('OK'),
        'go-unlock': () => { state.homeSelected = 'unlock'; navigate('unlock'); },
        'go-menu': () => { state.homeSelected = 'menu'; navigate('admin'); },
        'role-user': () => { state.addAdmin = false; state.addField = 2; rerender(); },
        'role-admin': () => { state.addAdmin = true; state.addField = 2; rerender(); },
        'enroll-fp': () => showFpModal('录入完成', state.addAcc.length > 0),
        'enroll-nfc': () => showNfcModal('绑定完成', state.addAcc.length > 0),
        'wifi-scan': wifiScan,
        'pair-regen': regenPair,
        'go-nfc': () => navigate('nfcMgmt'),
        'go-fp': () => navigate('fpMgmt'),
        'nfc-change': () => showNfcModal('更新完成', true),
        'nfc-delete': () => {
          const u = state.foundUser || MOCK_USERS[1];
          showAnimatedModal({
            title: u.nfc ? '已移除' : '清除失败',
            sub: u.nfc ? 'NFC 绑定' : '未绑定卡片',
            type: u.nfc ? 'clearOk' : 'clearFail',
          });
        },
        'fp-change': () => showFpModal('更新完成', true),
        'fp-delete': () => {
          const u = state.foundUser || MOCK_USERS[1];
          showAnimatedModal({
            title: u.fp ? '已清除' : '清除失败',
            sub: u.fp ? '指纹模板' : '未录入指纹',
            type: u.fp ? 'clearOk' : 'clearFail',
          });
        },
        noop: () => toast('预览模式'),
      };
      if (acts[a]) acts[a]();
    };
  }

  function buildSidebar() {
    const nav = $('#sidebar-nav');
    if (!nav) return;
    const groups = { main: '入口', auth: '验证', admin: '管理', sub: '凭证' };
    let html = '';
    for (const [gk, gt] of Object.entries(groups)) {
      const items = Object.values(SCREENS).filter(s => s.group === gk);
      if (!items.length) continue;
      html += `<div class="nav-group"><div class="nav-group-title">${gt}</div>`;
      items.forEach((s, i) => {
        html += `<button class="nav-btn" data-screen="${s.id}"><span class="idx">${String(i + 1).padStart(2, '0')}</span>${s.title}</button>`;
      });
      html += '</div>';
    }
    nav.innerHTML = html;
    nav.onclick = (e) => {
      const b = e.target.closest('.nav-btn');
      if (!b) return;
      state.prevStack = [];
      navigate(b.dataset.screen, false);
    };
  }

  function init() {
    buildSidebar();
    render();
    updateInfo();
    setInterval(() => {
      updateClock();
      if (state.screen === 'unlock' && state.lockoutUntil > 0) {
        if (state.lockoutUntil <= Date.now()) {
          state.lockoutUntil = 0;
          state.unlockErrors = 0;
        }
        rerender();
      }
    }, 1000);

    $('#key-hints')?.addEventListener('click', (e) => {
      const b = e.target.closest('.key-hint');
      if (b) handleKey(b.dataset.key);
    });

    $('.device-screen')?.addEventListener('dblclick', () => {
      if (state.screen === 'home') passiveUnlock();
    });

    if (globalThis.LockTouch) {
      LockTouch.init({
        state,
        handleBackSwipe,
        scrollSteps,
        wifiScan,
        patchHomeOnly,
      });
    }
  }

  document.readyState === 'loading'
    ? document.addEventListener('DOMContentLoaded', init)
    : init();
})();
