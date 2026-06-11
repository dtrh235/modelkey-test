/**
 * Joyful Zone — 清新 Material 风格共用组件
 */
const JZ_IMG = {
  emblem: 'assets/ycu_emblem.png',
  /** 本地智能门锁实拍（Unsplash 下载缓存） */
  lock: 'assets/smart_lock.jpg',
  /** 远程开锁页门锁图（本地资源，APK 内可离线显示） */
  lockRemote: 'assets/smart_lock.jpg',
};

/** Capacitor APK / 真机 WebView：全屏布局，不用浏览器原型手机框 */
function jzIsNativeApp() {
  if (window.Capacitor?.isNativePlatform?.()) return true;
  return location.protocol === 'https:' && location.hostname === 'localhost';
}

/** 当前页面文件名，用于导航 */
function jzCurrentPage() {
  const p = location.pathname.split('/').pop();
  return p || 'home.html';
}

function jzQuery(key) {
  return new URLSearchParams(location.search).get(key);
}

/** 带来源参数的链接，便于返回栈；ctx 保留更上一级的 from（如 首页→通知中心→设置） */
function jzHref(page, from) {
  const src = (from || jzCurrentPage()).replace(/\.html$/i, '');
  const dest = page.replace(/\.html$/i, '').split('?')[0];
  if (!src || src === dest) return page;
  const q = page.includes('?') ? '&' : '?';
  let url = `${page}${q}from=${encodeURIComponent(src)}`;
  const ctxFrom = jzQuery('from');
  if (ctxFrom && ctxFrom !== src) {
    url += `&ctx=${encodeURIComponent(ctxFrom)}`;
  }
  return url;
}

/** 根据 ?from= 解析返回页（「我的」下属设置页请用 initSettingsPage 的 backAnchor） */
function jzResolveBack(fallback) {
  const from = jzQuery('from');
  const ctx = jzQuery('ctx');
  const map = {
    home: 'home.html',
    devices: 'devices.html',
    device_detail: 'device_detail.html',
    unlock: 'unlock.html',
    records: 'records.html',
    users: 'users.html',
    notifications: 'notifications.html',
    profile: 'profile.html',
    unlock_settings: 'unlock_settings.html',
    avatar_picker: 'avatar_picker.html',
    temp_password: 'temp_password.html',
    theme: 'theme.html',
    security: 'security.html',
    login: 'login.html',
    add_device: 'devices.html',
    device_host: 'device_detail.html',
    device_client: 'device_detail.html',
    mqtt_settings: 'device_detail.html',
    app_bind: 'device_detail.html',
    enroll: 'users.html',
    user_detail: 'users.html',
    user_add: 'users.html',
    user_password: 'user_detail.html',
    record_detail: 'records.html',
    notification_detail: 'notifications.html',
    notify_settings: 'notify_settings.html',
    help: 'profile.html',
    about: 'profile.html',
    agreement: 'profile.html',
  };
  if (!from || !map[from]) return fallback;
  let target = map[from];
  if (ctx) {
    const q = target.includes('?') ? '&' : '?';
    target = `${target}${q}from=${encodeURIComponent(ctx)}`;
  }
  return target;
}

/** 时段问候语 */
function jzGreeting() {
  const h = new Date().getHours();
  if (h < 6) return '夜深了';
  if (h < 12) return '上午好';
  if (h < 14) return '中午好';
  if (h < 18) return '下午好';
  return '晚上好';
}

/** App 登录账号（原型 mock；接入后端后由登录接口写入 settings） */
function jzAppLoginAccount() {
  const s = jzLoadSettings();
  const acc = s.appLoginAccount;
  if (acc && jzIsNumericCred(acc, JZ_CRED.ACC_MIN_LEN, JZ_CRED.ACC_MAX_LEN)) {
    return acc;
  }
  return '1001';
}

/** 首页标题：账号 1001，上午好 */
function jzHomeGreetingTitle() {
  return `账号 ${jzAppLoginAccount()}，${jzGreeting()}`;
}

/** 门锁用户角色（无姓名字段，对齐 user_cred_t + is_admin） */
function jzUserRoleLabel(u) {
  if (!u) return '普通用户';
  if (u.isAdmin) return '管理员';
  if (u.isTemp) return '临时用户';
  return '普通用户';
}

function jzUserIsAdmin(u) {
  return !!(u && u.isAdmin);
}

function jzUserIsTemp(u) {
  return !!(u && u.isTemp);
}

/** 列表圆形头像：显示账号末 1–2 位数字 */
function jzUserAvatarText(u) {
  const acc = u && u.acc ? String(u.acc) : '?';
  return acc.length <= 2 ? acc : acc.slice(-2);
}

function jzEnsureBase() {
  if (!document.querySelector('base')) {
    const base = document.createElement('base');
    const dir = location.href.replace(/[#?].*$/, '').replace(/[^/]+$/, '');
    base.href = dir || './';
    document.head.prepend(base);
  }
  const viewport = document.querySelector('meta[name="viewport"]');
  if (viewport && !/viewport-fit=cover/.test(viewport.content)) {
    viewport.content += ', viewport-fit=cover';
  }
  jzInjectStyles();
  jzApplyUiMode();
  jzInitAppLifecycle();
}

function jzInjectStyles() {
  if (document.getElementById('jz-styles')) return;
  const s = document.createElement('style');
  s.id = 'jz-styles';
  s.textContent = `
    .jz-mode-adult { letter-spacing: 0.01em; }
    .jz-mode-adult .text-\\[10px\\] { font-size: 11px !important; }
    .jz-mode-adult .grid.grid-cols-3.gap-3 { gap: 0.65rem !important; }
    .jz-mode-adult .shadow-card { box-shadow: 0 1px 6px rgba(46,184,168,.1) !important; }
    .jz-mode-senior { font-size: 108%; }
    .jz-mode-senior .text-xs, .jz-mode-senior .text-\\[10px\\], .jz-mode-senior .text-\\[11px\\] { font-size: 13px !important; line-height: 1.45 !important; }
    .jz-mode-senior .text-sm { font-size: 16px !important; }
    .jz-mode-senior .text-base { font-size: 18px !important; }
    .jz-mode-senior .text-lg { font-size: 20px !important; }
    .jz-mode-senior .jz-tab-nav { height: 68px !important; }
    .jz-mode-senior .jz-tab-nav .text-\\[10px\\] { font-size: 12px !important; }
    .jz-mode-senior .w-10.h-10 { width: 3rem !important; height: 3rem !important; }
    .jz-mode-senior [data-jz-shell] { background: #fff !important; }
    .jz-mode-senior .text-ycu-navy { color: #0f172a !important; }
    .jz-mode-senior .shadow-card { box-shadow: 0 2px 8px rgba(0,0,0,.12) !important; }
    .jz-auth-modal { min-width: 17.5rem; max-width: calc(100vw - 2rem); }
    .jz-auth-modal h3,
    .jz-auth-modal .jz-modal-line { white-space: nowrap; text-align: center; }
    .jz-auth-modal .jz-modal-line,
    .jz-auth-modal #jzAuthErr { font-size: 0.75rem; line-height: 1.25rem; white-space: nowrap; }
    .jz-auth-modal input { font-size: 1.5rem; }
    .jz-mode-senior .jz-auth-modal input { font-size: 2rem; padding: 1rem; }
    .jz-toast { white-space: nowrap; max-width: calc(100vw - 2rem); overflow: hidden; text-overflow: ellipsis; }
    [data-jz-avatar] { border-radius: 9999px !important; aspect-ratio: 1 / 1; }
    a[data-jz-avatar-link] { border-radius: 9999px; line-height: 0; display: inline-block; }
    .jz-page-desc, .jz-hint, .jz-section-label { text-wrap: pretty; word-break: normal; }
    .jz-hint { line-height: 1.5; }
    html.jz-native, html.jz-native body { height: 100%; margin: 0; }
    html.jz-native body { overflow: hidden; }
    @keyframes jz-nfc-card-move {
      0%, 100% { transform: translate(0, 0) rotate(-6deg); opacity: 1; }
      45% { transform: translate(52px, 8px) rotate(2deg); opacity: 1; }
      55% { transform: translate(58px, 10px) rotate(4deg); opacity: .85; }
      70% { transform: translate(52px, 8px) rotate(2deg); opacity: 1; }
    }
    @keyframes jz-nfc-reader-pulse {
      0%, 100% { box-shadow: 0 0 0 0 rgba(46,184,168,.45); }
      50% { box-shadow: 0 0 0 10px rgba(46,184,168,0); }
    }
    @keyframes jz-nfc-wave {
      0% { transform: scale(.6); opacity: .7; }
      100% { transform: scale(1.35); opacity: 0; }
    }
    @keyframes jz-step-active {
      0%, 100% { opacity: 1; }
      33% { opacity: .35; }
      66% { opacity: .35; }
    }
    .jz-nfc-anim-card { animation: jz-nfc-card-move 2.8s ease-in-out infinite; }
    .jz-nfc-anim-reader { animation: jz-nfc-reader-pulse 2s ease-in-out infinite; }
    .jz-nfc-anim-wave { animation: jz-nfc-wave 2s ease-out infinite; }
    .jz-enroll-step-dot[data-active="1"] { animation: jz-step-active 3s ease-in-out infinite; }
    .jz-enroll-step-dot[data-active="2"] { animation: jz-step-active 3s ease-in-out infinite .95s; }
    .jz-enroll-step-dot[data-active="3"] { animation: jz-step-active 3s ease-in-out infinite 1.9s; }
    .jz-tab-nav a { transition: color .2s ease; }
    .jz-tab-line {
      transition: width .22s ease, opacity .22s ease;
      background: linear-gradient(to top, #B8EDE4, #3ECFB4);
    }
    .jz-tab-scroll {
      position: relative;
      overflow-y: auto;
      -webkit-overflow-scrolling: touch;
      overscroll-behavior-y: contain;
      touch-action: pan-y;
      scrollbar-width: none;
      -ms-overflow-style: none;
    }
    .jz-tab-scroll::-webkit-scrollbar { display: none; }
    .hide-scroll { scrollbar-width: none; -ms-overflow-style: none; }
    .hide-scroll::-webkit-scrollbar { display: none; }
    [data-jz-shell] .overflow-y-auto,
    [data-jz-shell] .overflow-x-auto {
      scrollbar-width: none;
      -ms-overflow-style: none;
    }
    [data-jz-shell] .overflow-y-auto::-webkit-scrollbar,
    [data-jz-shell] .overflow-x-auto::-webkit-scrollbar { display: none; }
    .jz-tab-scroll-inner { min-height: 100%; will-change: transform; }
    .jz-tab-scroll:not(.jz-pull-dragging) .jz-tab-scroll-inner {
      transition: transform 0.28s cubic-bezier(0.25, 0.46, 0.45, 0.94);
    }
    .jz-pull-indicator {
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      z-index: 5;
      height: 0;
      overflow: hidden;
      display: flex;
      align-items: flex-end;
      justify-content: center;
      padding-bottom: 8px;
      color: #009B8E;
      font-size: 12px;
      pointer-events: none;
    }
    .jz-pull-icon { transition: transform 0.2s ease; font-size: 14px; }
    .jz-tab-scroll.jz-pull-ready .jz-pull-icon { transform: rotate(180deg); }
    .jz-tab-scroll.jz-pull-loading .jz-pull-icon { animation: spin 0.8s linear infinite; }
    @keyframes spin { to { transform: rotate(360deg); } }
    .jz-onboard-viewport { touch-action: pan-y pinch-zoom; }
    .jz-onboard-track { display: flex; height: 100%; will-change: transform; }
    .jz-onboard-slide { flex: 0 0 100%; width: 100%; display: flex; flex-direction: column; align-items: center; padding: 0 2rem; text-align: center; }
  `;
  document.head.appendChild(s);
}

/** 账号数字升序（与门锁 screen4 / 后端 userStore 一致） */
function jzAccountSortKey(acc) {
  const s = String(acc || '').trim();
  if (/^\d+$/.test(s)) {
    try {
      return BigInt(s);
    } catch {
      return 0n;
    }
  }
  return 0n;
}

function jzSortUsersByAccount(users) {
  return [...(users || [])].sort((a, b) => {
    const ka = jzAccountSortKey(a.acc);
    const kb = jzAccountSortKey(b.acc);
    if (ka < kb) return -1;
    if (ka > kb) return 1;
    return String(a.acc).localeCompare(String(b.acc));
  });
}

function jzUsersFromCache() {
  const cache = jzLoadSettings().usersCache;
  return cache && Array.isArray(cache.users) ? jzSortUsersByAccount(cache.users) : [];
}

function jzGetUser(idOrAcc) {
  if (idOrAcc == null || idOrAcc === '') return null;
  const key = String(idOrAcc);
  const users = jzUsersFromCache();
  const byId = users.find((u) => String(u.id) === key);
  if (byId) return byId;
  return users.find((u) => String(u.acc) === key) || null;
}

/** @deprecated 使用 jzGetUser */
function jzGetSampleUser(id) {
  return jzGetUser(id);
}

/** 智能门锁实拍图（在线优先，本地 assets 作离线回退） */
function renderLockImage(wrapperClass = 'w-full h-44 overflow-hidden bg-slate-100 rounded-2xl') {
  const remote = JZ_IMG.lockRemote.replace(/'/g, '%27');
  const local = JZ_IMG.lock.replace(/'/g, '%27');
  return `
    <div class="${wrapperClass}" role="img" aria-label="智能指纹门锁">
      <img src="${remote}" alt="智能门锁" class="w-full h-full object-cover"
        loading="lazy"
        onerror="this.onerror=null;this.src='${local}'"/>
    </div>`;
}

/* ========== 用户设置（localStorage） ========== */
const JZ_SETTINGS_KEY = 'jz_app_settings_v1';
const JZ_AVATARS = [
  { bg: 'from-ycu-teal to-ycu-teal-dark', icon: 'fa-user' },
  { bg: 'from-ycu-blue to-indigo-500', icon: 'fa-user-tie' },
  { bg: 'from-amber-400 to-orange-500', icon: 'fa-user-graduate' },
  { bg: 'from-pink-400 to-rose-500', icon: 'fa-user-nurse' },
  { bg: 'from-violet-400 to-purple-600', icon: 'fa-user-astronaut' },
  { bg: 'from-emerald-400 to-teal-600', icon: 'fa-user-check' },
];
const JZ_UI_MODES = {
  standard: { label: '标准模式', desc: '默认布局与字号' },
  adult: { label: '成人模式', desc: '布局更紧凑' },
  senior: { label: '老年模式', desc: '大字号、高对比、大按钮' },
};

/** 对齐 Host/User/user_auth_portable.h · screen_1/screen_8 · ADMIN_DEFAULT 1/1111 */
const JZ_CRED = {
  ACC_MIN_LEN: 1,
  ACC_MAX_LEN: 12,
  PWD_MIN_LEN: 4,
  PWD_MAX_LEN: 10,
  TEMP_ACC_LEN: 4,
  TEMP_PWD_LEN: 6,
  TEMP_VALID_MS: 5 * 60 * 1000,
  DEFAULT_ADMIN_ACC: '1',
  DEFAULT_ADMIN_PWD: '1111',
};

function jzIsNumericCred(str, minLen, maxLen) {
  return typeof str === 'string' && /^\d+$/.test(str) && str.length >= minLen && str.length <= maxLen;
}

function jzRandomDigits(len, noLeadingZero = false) {
  let s = '';
  for (let i = 0; i < len; i++) {
    if (i === 0 && noLeadingZero) {
      s += String(Math.floor(Math.random() * 9) + 1);
    } else {
      s += String(Math.floor(Math.random() * 10));
    }
  }
  return s;
}

function jzTakenLockAccounts(extra = []) {
  const s = jzLoadSettings();
  const set = new Set([
    JZ_CRED.DEFAULT_ADMIN_ACC,
    '2', /* Client/User/main.c 测试用户 */
    ...extra,
    ...s.tempPasswords.filter((t) => t && !t.used && t.expiresAt > Date.now()).map((t) => t.account),
    ...jzUsersFromCache().map((u) => u.acc),
  ]);
  return set;
}

function jzSanitizeTempPasswords(list) {
  if (!Array.isArray(list)) return [];
  const now = Date.now();
  return list.filter((t) =>
    t &&
    jzIsNumericCred(t.account, JZ_CRED.ACC_MIN_LEN, JZ_CRED.ACC_MAX_LEN) &&
    jzIsNumericCred(t.password, JZ_CRED.PWD_MIN_LEN, JZ_CRED.PWD_MAX_LEN) &&
    typeof t.expiresAt === 'number'
  ).map((t) => ({
    ...t,
    isAdmin: false,
    active: t.active !== false,
    used: !!t.used || t.expiresAt <= now,
  }));
}

function jzDefaultSettings() {
  return {
    avatar: 0,
    uiMode: 'standard',
    theme: 'fresh',
    appLoginAccount: '1001',
    unlockSecondAuth: false,
    unlockSecondPin: '',
    tempPasswords: [],
    notifyPush: true,
    notifyUnlock: true,
    notifyRemote: true,
    notifyAlert: true,
    notifyDevice: true,
    notifySystem: false,
    notifySound: true,
    notifyVibrate: true,
    notifyReadIds: [],
    /** 绑定时刻；此前历史通知不计未读（仅首次绑定时写入，拉取通知不再改写） */
    notifyUnreadSince: 0,
    _notifyUnreadFix: 2,
    deviceBind: null,
    unlockRecordsCache: null,
    usersCache: null,
    notifyCache: null,
  };
}

/** App 展示最近 N 个月开锁记录（云端保留全部） */
const JZ_RECORD_MONTHS = 3;

/**
 * 真机 APK 默认连电脑上的 JoyfulZone/server（与手机同一 WiFi）。
 * 换网络或换电脑时：在「添加设备」页改后端地址，或改此常量后重新 cap:sync 打 APK。
 */
const JZ_DEFAULT_LAN_API_BASE = 'http://192.168.126.244:3000';

/** 后端 API 根地址（本机调试默认 3000；APK 默认局域网 IP） */
function jzApiBase() {
  const saved = localStorage.getItem('jzApiBase');
  if (saved) return saved.replace(/\/$/, '');
  if (jzIsNativeApp()) return JZ_DEFAULT_LAN_API_BASE;
  if (location.protocol === 'file:') return 'http://127.0.0.1:3000';
  if (location.hostname === 'localhost' || location.hostname === '127.0.0.1') {
    return `${location.protocol}//${location.hostname}:3000`;
  }
  return 'http://127.0.0.1:3000';
}

function jzSaveApiBase(url) {
  const trimmed = String(url || '').trim().replace(/\/$/, '');
  if (!trimmed) {
    localStorage.removeItem('jzApiBase');
    return jzApiBase();
  }
  localStorage.setItem('jzApiBase', trimmed);
  return trimmed;
}

/** 探测后端是否可达（添加设备页「测试连接」用） */
async function jzPingApiBase(base) {
  const url = (base || jzApiBase()).replace(/\/$/, '');
  const resp = await fetch(`${url}/health`, { method: 'GET' });
  const data = await resp.json().catch(() => ({}));
  return { ok: resp.ok && data.ok, url, data };
}

/** 当前 App 是否已绑定门锁（含离线；配对一次后持久保存） */
function jzDeviceBindInfo() {
  return jzLoadSettings().deviceBind || null;
}

/** 列表/首页是否展示已绑定设备（与在线状态无关） */
function jzDeviceBindInfoForUI() {
  return jzDeviceBindInfo();
}

/** 需要门锁云端在线才能执行的操作（远程开锁等） */
function jzDeviceBindOnline() {
  const bind = jzDeviceBindInfo();
  if (!bind || !jzLockIsOnline(bind)) return null;
  return bind;
}

let jzBindSyncInFlight = false;
let jzLastPresenceUiKey = '';

/** 与后端 GetDeviceStatus 轮询（3s）+ 缓存（8s）对齐，目标 ~10s 内感知离线 */
/** 与固件 presence 25s + 后端 uplink 30s 对齐 */
const JZ_LOCK_PRESENCE_STALE_MS = 35 * 1000;

function jzLockPresenceStaleMs() {
  const bind = jzDeviceBindInfo();
  if (!bind || !bind.lockLastSeenAt) return 0;
  return Math.max(0, Date.now() - Number(bind.lockLastSeenAt));
}

/** 本地根据 lastSeen 预判离线（不必等后端 35s 轮询才变灰） */
function jzLockPresenceLooksStale() {
  const bind = jzDeviceBindInfo();
  if (!bind) return false;
  if (bind.lockLwtOffline) return true;
  if (bind.lockOnline === true) return false;
  if (!bind.lockLastSeenAt) return false;
  return jzLockPresenceStaleMs() >= JZ_LOCK_PRESENCE_STALE_MS;
}

/** unbound | bound_online | bound_offline */
function jzConnectionState() {
  const bind = jzDeviceBindInfo();
  if (!bind) return 'unbound';
  if (jzLockIsOnline(bind)) return 'bound_online';
  return 'bound_offline';
}

/** 门锁在线/离线均 2s 轮询，配合后端 3s API 轮询 */
function jzPresencePollDelayMs() {
  const bind = jzDeviceBindInfo();
  if (!bind) return 12000;
  return 2000;
}

/** 不访问网络，仅按 lastSeen 刷新在线/离线 UI（2s 本地时钟） */
function jzTickPresenceUi() {
  const bind = jzDeviceBindInfo();
  if (!bind) {
    jzLastPresenceUiKey = '';
    return;
  }
  const uiKey = `${jzConnectionState()}:${Math.floor(jzLockPresenceStaleMs() / 1000)}`;
  if (uiKey === jzLastPresenceUiKey) return;
  jzLastPresenceUiKey = uiKey;
  jzFireBindChange({
    bound: true,
    boundStored: true,
    cleared: false,
    lockOnline: jzLockIsOnline(bind),
    connectionState: jzConnectionState(),
    presenceChanged: true,
    source: 'ui-tick',
  });
}

function jzDeviceConnectionMeta() {
  switch (jzConnectionState()) {
    case 'bound_online':
      return {
        stripLine: '已绑定 · 门锁在线',
        textClass: 'text-emerald-600',
        dotClass: 'bg-emerald-400 animate-pulse',
        badgeLabel: '在线',
        badgeClass: 'bg-emerald-50 text-emerald-600 border-emerald-100',
        cloudText: '门锁云端已连接',
        cloudClass: 'text-emerald-600 font-medium',
      };
    default:
      return {
        stripLine: '已绑定 · 门锁离线',
        textClass: 'text-slate-500',
        dotClass: 'bg-slate-300',
        badgeLabel: '离线',
        badgeClass: 'bg-slate-50 text-slate-500 border-slate-200',
        cloudText: '门锁云端未连接',
        cloudClass: 'text-slate-400 font-medium',
      };
  }
}

/** 使用 6 位配对码绑定设备（经后端 → 阿里云 → 门锁） */
async function jzBindWithPairCode(code) {
  const trimmed = String(code || '').trim();
  if (!/^\d{6}$/.test(trimmed)) {
    showToast('请输入 6 位配对码');
    return false;
  }
  try {
    const resp = await fetch(`${jzApiBase()}/devices/bind`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        pairCode: trimmed,
        appAccount: jzAppLoginAccount(),
      }),
    });
    const data = await resp.json().catch(() => ({}));
    if (!resp.ok || !data.ok) {
      const hint = data.error === 'BIND_ACK_TIMEOUT'
        ? '门锁未响应'
        : (data.message || '绑定失败');
      showToast(hint, data.error === 'BIND_ACK_TIMEOUT' ? 'error' : undefined);
      return false;
    }
    const boundAt = Date.now();
    jzSaveSettings({
      deviceBind: {
        productKey: data.productKey,
        deviceName: data.deviceName,
        appAccount: data.appAccount,
        deviceLabel: '实验室智能门锁',
        boundAt,
      },
      notifyUnreadSince: boundAt,
      notifyReadIds: [],
      _notifyUnreadFix: 2,
    });
    jzLastBindKey = jzBindStateKey();
    jzFireBindChange({ bound: true, cleared: false, source: 'bind' });
    showToast(data.message || '绑定成功');
    jzSyncDeviceBindStatus().catch(() => {});
    jzFetchUsers({ refresh: true }).catch(() => {});
    return true;
  } catch {
    const base = jzApiBase();
    showToast('无法连接后端，请确认服务已启动');
    return false;
  }
}

/** 远程开锁（经后端 → 阿里云 → 门锁） */
async function jzRemoteUnlock() {
  const bind = jzDeviceBindInfo();
  if (!bind) {
    showToast('请先在设备管理中绑定门锁');
    return { ok: false };
  }
  if (!jzDevicePresenceOnline()) {
    showToast('门锁离线，请稍后再试', 'error');
    return { ok: false };
  }
  try {
    const resp = await fetch(`${jzApiBase()}/devices/unlock`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        appAccount: bind.appAccount || jzAppLoginAccount(),
        deviceName: bind.deviceName,
      }),
    });
    const data = await resp.json().catch(() => ({}));
    if (!resp.ok || !data.ok) {
      showToast(data.message || '远程开锁失败');
      return { ok: false, data };
    }
    showToast(data.message || '开锁成功');
    return { ok: true, data };
  } catch {
    const base = jzApiBase();
    showToast('无法连接后端');
    return { ok: false };
  }
}

function jzUnlockMethodLabel(code) {
  return { 1: '密码', 2: 'NFC', 3: '指纹', 4: '手机', 5: '临时' }[Number(code)] || '未知';
}

function jzUnlockAccountLabel(account, methodCode) {
  if (Number(methodCode) === 4) return '0';
  if (Number(methodCode) === 5) return 'temporary account';
  const acc = String(account || '').trim();
  return acc || '未知';
}

/** 列表主标题：手机远程 vs 账号+方式 */
function jzRecordTitleLine(view) {
  if (Number(view.unlockMethod) === 4 || view.m === '手机') {
    return '手机远程开锁';
  }
  if (Number(view.unlockMethod) === 5 || view.m === '临时') {
    return '临时密码开锁';
  }
  const u = view.u === '未知' ? '未知' : `账号 ${view.u}`;
  return `${u} · ${view.m}开锁`;
}

function jzUnlockDeviceLabel(deviceCode, methodCode) {
  if (Number(deviceCode) === 3 || Number(methodCode) === 4) return '手机 App';
  return Number(deviceCode) === 2 ? '侧门' : '主屏';
}

function jzRecordHm(ts, unlockTime) {
  const s = String(unlockTime || '').trim();
  const m = s.match(/(\d{2}):(\d{2})/);
  if (m) return `${m[1]}:${m[2]}`;
  const d = new Date(ts || Date.now());
  const pad = (n) => String(n).padStart(2, '0');
  return `${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

/** 日期分组标题：今天 / 昨天 / 3月15日 */
function jzRecordDayLabel(ts) {
  const d = new Date(ts || Date.now());
  const now = new Date();
  const pad = (n) => String(n).padStart(2, '0');
  const today0 = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
  const day0 = new Date(d.getFullYear(), d.getMonth(), d.getDate()).getTime();
  const diff = Math.round((today0 - day0) / 86400000);
  if (diff === 0) return '今天';
  if (diff === 1) return '昨天';
  if (diff > 1 && diff < 7) return `${diff}天前`;
  if (d.getFullYear() === now.getFullYear()) return `${d.getMonth() + 1}月${d.getDate()}日`;
  return `${d.getFullYear()}年${d.getMonth() + 1}月${d.getDate()}日`;
}

function jzRecordDayKey(ts) {
  const d = new Date(ts || Date.now());
  return `${d.getFullYear()}-${pad2(d.getMonth())}-${pad2(d.getDate())}`;
  function pad2(n) { return String(n).padStart(2, '0'); }
}

function jzFormatRecordRelative(ts, unlockTime) {
  return `${jzRecordDayLabel(ts)} ${jzRecordHm(ts, unlockTime)}`;
}

/** 按天分组（米家/华为智慧生活同类：一天一个标题） */
function jzGroupRecordViewsByDay(views) {
  const groups = [];
  const map = new Map();
  for (const v of views) {
    const key = jzRecordDayKey(v.ts);
    if (!map.has(key)) {
      const g = { key, label: jzRecordDayLabel(v.ts), items: [] };
      map.set(key, g);
      groups.push(g);
    }
    map.get(key).items.push(v);
  }
  return groups;
}

function jzRecordToView(rec) {
  const method = jzUnlockMethodLabel(rec.unlockMethod);
  return {
    id: rec.id,
    m: method,
    u: jzUnlockAccountLabel(rec.unlockAccount, rec.unlockMethod),
    t: jzFormatRecordRelative(rec.ts, rec.unlockTime),
    hm: jzRecordHm(rec.ts, rec.unlockTime),
    day: jzRecordDayLabel(rec.ts),
    d: jzUnlockDeviceLabel(rec.unlockDevice, rec.unlockMethod),
    ok: rec.ok !== false ? 1 : 0,
    unlockTime: rec.unlockTime,
    ts: rec.ts,
    unlockMethod: rec.unlockMethod,
    unlockDevice: rec.unlockDevice,
  };
}

function jzUnlockRecordsFromCache() {
  const cache = jzLoadSettings().unlockRecordsCache;
  return cache && Array.isArray(cache.records) ? cache.records : [];
}

let jzLastUsersForceRefreshAt = 0;

function jzUsersListSignature(list) {
  return jzListSignature(list || [], (u) =>
    `${u.acc}:${u.isAdmin ? 1 : 0}:${u.pwd ? 1 : 0}:${u.nfc ? 1 : 0}:${u.fp ? 1 : 0}`
  );
}

/** 用户列表有增删改时通知各页面从本地缓存重绘 */
function jzNotifyUsersChanged() {
  jzEmitAppEvent('jz-users-changed', {});
}

let jzLastTempPasswordPullAt = 0;
let jzLastTempPasswordSig = '';

function jzTempPasswordListSig(list) {
  return jzListSignature(list, (t) => `${t.id}:${t.used ? 1 : 0}:${t.expiresAt}`);
}

/** 从后端拉有效临时密码；有变化时发 jz-temp-passwords-changed */
async function jzPullTempPasswordsIfDue(force = false) {
  const bind = jzDeviceBindInfo();
  if (!bind) return false;
  const now = Date.now();
  if (!force && now - jzLastTempPasswordPullAt < 2000) return false;
  jzLastTempPasswordPullAt = now;
  try {
    const list = await jzFetchTempPasswords();
    const sig = jzTempPasswordListSig(list);
    if (!force && sig === jzLastTempPasswordSig) return false;
    jzLastTempPasswordSig = sig;
    jzSaveSettings({ tempPasswords: list });
    jzEmitAppEvent('jz-temp-passwords-changed', { items: list });
    return true;
  } catch {
    return false;
  }
}

function jzNotifyTempPasswordsChanged() {
  jzEmitAppEvent('jz-temp-passwords-changed', {
    items: jzPruneTempPasswords(jzLoadSettings().tempPasswords),
  });
}

/** 从后端已同步的 lock_users 更新 App 缓存（门锁 user_changed 上报后） */
async function jzPullUsersFromBackend() {
  const bind = jzDeviceBindInfo();
  if (!bind) return false;
  const prev = jzUsersListSignature(jzUsersFromCache());
  const list = await jzFetchUsers({ network: true });
  const changed = prev !== jzUsersListSignature(list);
  if (changed) {
    jzNotifyUsersChanged();
  }
  return changed;
}

/**
 * 用户列表数据
 * - 默认：只读本地缓存，不请求网络（界面静态展示）
 * - network: true：读后端已存列表，不触发门锁 sync_users
 * - refresh: true：与门锁同步（仅下拉刷新等用户主动操作）
 */
async function jzFetchUsers(opts = {}) {
  const bind = jzDeviceBindInfo();
  if (!bind) return [];
  if (!opts.refresh && !opts.network) {
    return jzUsersFromCache();
  }
  const deviceName = opts.deviceName || bind.deviceName || '1111';
  if (opts.refresh) {
    const now = Date.now();
    if (now - jzLastUsersForceRefreshAt < 5000) {
      return jzUsersFromCache();
    }
    jzLastUsersForceRefreshAt = now;
  }
  const refresh = opts.refresh ? '&refresh=1' : '';
  const timeoutMs = opts.refresh ? 12000 : 5000;
  try {
    const data = await jzFetchJson(
      `${jzApiBase()}/users?deviceName=${encodeURIComponent(deviceName)}${refresh}`,
      {},
      timeoutMs
    );
    if (data.ok && Array.isArray(data.users)) {
      const sorted = jzSortUsersByAccount(data.users);
      jzSaveSettings({ usersCache: { at: Date.now(), deviceName, users: sorted } });
      return sorted;
    }
  } catch {
    /* 离线用缓存 */
  }
  return jzUsersFromCache();
}

async function jzCreateUser({ account, password, isAdmin = false }) {
  const bind = jzDeviceBindInfo();
  if (!bind) {
    showToast('请先在设备管理中绑定门锁', 'error');
    return { ok: false };
  }
  try {
    const resp = await fetch(`${jzApiBase()}/users`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        deviceName: bind.deviceName,
        account,
        password,
        isAdmin,
      }),
    });
    const data = await resp.json().catch(() => ({}));
    if (!resp.ok || !data.ok) {
      showToast(data.message || '添加用户失败', 'error');
      return { ok: false, data };
    }
    const users = jzUsersFromCache().filter((u) => u.acc !== account);
    users.push(data.user);
    jzSaveSettings({
      usersCache: { at: Date.now(), deviceName: bind.deviceName, users: jzSortUsersByAccount(users) },
    });
    jzFetchNotifications({ baseline: false }).catch(() => {});
    jzNotifyUsersChanged();
    return { ok: true, user: data.user };
  } catch {
    showToast('无法连接后端', 'error');
    return { ok: false };
  }
}

async function jzDeleteUser(account) {
  const bind = jzDeviceBindInfo();
  if (!bind) {
    showToast('请先在设备管理中绑定门锁', 'error');
    return { ok: false };
  }
  try {
    const resp = await fetch(
      `${jzApiBase()}/users/${encodeURIComponent(account)}?deviceName=${encodeURIComponent(bind.deviceName)}`,
      { method: 'DELETE' }
    );
    const data = await resp.json().catch(() => ({}));
    if (!resp.ok || !data.ok) {
      showToast(data.message || '删除失败', 'error');
      return { ok: false, data };
    }
    const users = jzUsersFromCache().filter((u) => u.acc !== account);
    jzSaveSettings({
      usersCache: { at: Date.now(), deviceName: bind.deviceName, users: jzSortUsersByAccount(users) },
    });
    jzFetchNotifications({ baseline: false }).catch(() => {});
    jzNotifyUsersChanged();
    return { ok: true };
  } catch {
    showToast('无法连接后端', 'error');
    return { ok: false };
  }
}

async function jzUpdateUserPassword(account, password, oldPassword) {
  const bind = jzDeviceBindInfo();
  if (!bind) {
    showToast('请先在设备管理中绑定门锁', 'error');
    return { ok: false };
  }
  try {
    const body = { deviceName: bind.deviceName, password };
    if (oldPassword) body.oldPassword = oldPassword;
    const resp = await fetch(`${jzApiBase()}/users/${encodeURIComponent(account)}/password`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    const data = await resp.json().catch(() => ({}));
    if (!resp.ok || !data.ok) {
      const msg = data.error === 'OLD_PASSWORD_REQUIRED'
        ? '修改管理员密码须填写原密码'
        : (data.message || '密码更新失败');
      showToast(msg, 'error');
      return { ok: false, data };
    }
    const users = jzUsersFromCache().map((u) =>
      (u.acc === account ? { ...u, pwd: true } : u)
    );
    jzSaveSettings({
      usersCache: { at: Date.now(), deviceName: bind.deviceName, users: jzSortUsersByAccount(users) },
    });
    jzFetchNotifications({ baseline: false }).catch(() => {});
    jzNotifyUsersChanged();
    return { ok: true, user: data.user };
  } catch {
    showToast('无法连接后端', 'error');
    return { ok: false };
  }
}

/** 带超时的 fetch，避免后端不可达时白屏卡住 */
async function jzFetchJson(url, opts = {}, timeoutMs = 4500) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const resp = await fetch(url, { ...opts, signal: ctrl.signal });
    return await resp.json();
  } finally {
    clearTimeout(timer);
  }
}

/** —— 前台恢复 / 绑定变化自动刷新（参考米家：回前台同步、断连清缓存） —— */
let jzLifecycleStarted = false;
let jzLastBindKey = null;
const jzResumeHandlers = new Set();
const jzBindHandlers = new Set();

function jzLockIsOnline(bind) {
  if (!bind) return false;
  if (bind.lockLwtOffline) return false;
  return bind.lockOnline === true;
}

function jzDevicePresenceOnline() {
  return jzConnectionState() === 'bound_online';
}

function jzDevicePresenceBadgeHtml(opts = {}) {
  const meta = jzDeviceConnectionMeta();
  const size = opts.large ? 'text-xs font-semibold border px-2.5 py-1 rounded-lg' : 'text-xs font-medium px-2 py-1 rounded-lg';
  if (jzConnectionState() === 'unbound') {
    return `<span class="${size} bg-slate-50 text-slate-400 border-slate-100">未绑定</span>`;
  }
  return `<span class="${size} ${meta.badgeClass} border">${meta.badgeLabel}</span>`;
}

function jzDevicePresenceRowBadge() {
  const state = jzConnectionState();
  if (state === 'bound_online') return '在线';
  if (state === 'unbound') return '未绑定';
  return '离线';
}

/** 远程开锁页：在线/离线 + 锁状态一行 */
function jzUnlockPresenceLineHtml() {
  const online = jzDevicePresenceOnline();
  const dot = online ? 'bg-emerald-400' : 'bg-slate-300';
  const cls = online ? 'text-emerald-600' : 'text-slate-400';
  const label = online ? '在线' : '离线';
  return `<p id="unlockPresenceLine" class="text-xs ${cls} mt-0.5"><span class="inline-block w-1.5 h-1.5 rounded-full ${dot} mr-1 align-middle"></span>${label} · 已上锁</p>`;
}

function jzPaintUnlockPresence() {
  const el = document.getElementById('unlockPresenceLine');
  const btn = document.getElementById('holdBtn');
  const hint = document.getElementById('unlockOfflineHint');
  const online = jzDevicePresenceOnline();
  if (el) {
    const dot = online ? 'bg-emerald-400' : 'bg-slate-300';
    const cls = online ? 'text-emerald-600' : 'text-slate-400';
    const label = online ? '在线' : '离线';
    el.className = `text-xs ${cls} mt-0.5`;
    el.innerHTML = `<span class="inline-block w-1.5 h-1.5 rounded-full ${dot} mr-1 align-middle"></span>${label} · 已上锁`;
  }
  if (btn) {
    btn.classList.toggle('opacity-40', !online);
    btn.classList.toggle('pointer-events-none', !online);
    btn.setAttribute('aria-disabled', online ? 'false' : 'true');
  }
  if (hint) {
    hint.classList.toggle('hidden', online);
  }
}

/** 设备详情/子页：根据后端 presence 刷新连接状态 */
function jzPaintDevicePresence(ids = {}) {
  const meta = jzDeviceConnectionMeta();
  const online = jzDevicePresenceOnline();
  const main = document.getElementById(ids.main || 'deviceMainPresence');
  const host = document.getElementById(ids.host || 'deviceHostPresence');
  const client = document.getElementById(ids.client || 'deviceClientPresence');
  const cloud = document.getElementById(ids.cloud || 'deviceCloudSub');
  const paint = (el, asBadge) => {
    if (!el) return;
    if (asBadge) {
      el.innerHTML = jzDevicePresenceBadgeHtml({ large: true });
    } else {
      el.textContent = online ? '云端在线' : '云端离线';
      el.className = online ? 'text-emerald-600 font-medium' : 'text-slate-400 font-medium';
    }
  };
  paint(main, true);
  paint(host, false);
  paint(client, false);
  if (cloud) {
    cloud.textContent = meta.cloudText;
    cloud.className = meta.cloudClass;
  }
}

function jzWatchDevicePresence(ids) {
  const paint = () => jzPaintDevicePresence(ids);
  paint();
  return jzWatchPageRefresh({
    onResume: async () => {
      await jzSyncDeviceBindStatus();
      paint();
    },
    onBindChange: () => paint(),
    adaptivePresence: true,
    onPoll: async () => {
      await jzSyncDeviceBindStatus();
      paint();
    },
  });
}

function jzBindStateKey() {
  const raw = jzDeviceBindInfo();
  if (!raw) return '';
  return `${raw.deviceName || ''}:${raw.boundAt || ''}:${jzConnectionState()}`;
}

function jzEmitAppEvent(name, detail = {}) {
  try {
    window.dispatchEvent(new CustomEvent(name, { detail }));
  } catch {
    /* ignore */
  }
}

function jzClearDeviceRemoteCaches() {
  jzSaveSettings({
    deviceBind: null,
    usersCache: null,
    unlockRecordsCache: null,
    notifyCache: null,
    tempPasswords: [],
  });
}

function jzOnAppResume(fn) {
  jzResumeHandlers.add(fn);
  return () => jzResumeHandlers.delete(fn);
}

function jzOnBindChange(fn) {
  jzBindHandlers.add(fn);
  return () => jzBindHandlers.delete(fn);
}

function jzFireAppResume() {
  jzResumeHandlers.forEach((fn) => {
    try {
      fn();
    } catch {
      /* ignore */
    }
  });
  jzEmitAppEvent('jz-app-resume', {});
}

function jzFireBindChange(detail) {
  jzBindHandlers.forEach((fn) => {
    try {
      fn(detail);
    } catch {
      /* ignore */
    }
  });
  jzEmitAppEvent('jz-bind-changed', detail);
}

function jzStartPolling(fn, intervalMs = 5000) {
  const timer = setInterval(() => {
    try {
      fn();
    } catch {
      /* ignore */
    }
  }, intervalMs);
  return () => clearInterval(timer);
}

/** 按在线状态自动调节轮询间隔（离线 3s，在线 12s） */
function jzStartAdaptivePolling(fn) {
  let stopped = false;
  let timer = null;
  const schedule = (delayMs) => {
    if (stopped) return;
    timer = setTimeout(tick, delayMs);
  };
  const tick = async () => {
    if (stopped) return;
    try {
      await fn();
    } catch {
      /* ignore */
    }
    schedule(jzPresencePollDelayMs());
  };
  tick();
  return () => {
    stopped = true;
    if (timer) clearTimeout(timer);
  };
}

function jzListSignature(list, mapper) {
  return (list || []).map(mapper || ((x) => JSON.stringify(x))).join('|');
}

/** 页面级：前台恢复 / 绑定变化 / 定时轮询 */
function jzWatchPageRefresh(opts = {}) {
  const stops = [];
  if (typeof opts.onResume === 'function') {
    stops.push(jzOnAppResume(opts.onResume));
  }
  if (typeof opts.onBindChange === 'function') {
    stops.push(jzOnBindChange(opts.onBindChange));
  }
  if (typeof opts.onPoll === 'function') {
    if (opts.adaptivePresence) {
      stops.push(jzStartAdaptivePolling(opts.onPoll));
    } else if (opts.pollMs > 0) {
      stops.push(jzStartPolling(opts.onPoll, opts.pollMs));
    }
  }
  const cleanup = () => stops.forEach((stop) => stop());
  window.addEventListener('beforeunload', cleanup, { once: true });
  return cleanup;
}

function jzInitAppLifecycle() {
  if (jzLifecycleStarted) return;
  jzLifecycleStarted = true;
  jzLastBindKey = jzBindStateKey();

  jzOnBindChange((d) => {
    if (d.cleared) {
      jzClearDeviceRemoteCaches();
      jzEmitAppEvent('jz-bind-cleared', {});
    }
  });

  const onVisible = () => {
    if (document.visibilityState && document.visibilityState !== 'visible') return;
    jzFireAppResume();
    jzCheckBindStateChange('resume');
  };
  document.addEventListener('visibilitychange', onVisible);
  window.addEventListener('pageshow', onVisible);
  window.addEventListener('focus', onVisible);
  jzStartAdaptivePolling(() => jzCheckBindStateChange('poll'));
  jzStartPolling(() => jzTickPresenceUi(), 1000);
  jzStartPolling(() => {
    if (jzDeviceBindInfo()) jzPullTempPasswordsIfDue(false);
  }, 2000);
}

async function jzCheckBindStateChange(source = 'poll') {
  const prevKey = jzLastBindKey;
  const sync = await jzSyncDeviceBindStatus();
  let tempPasswordsChanged = false;
  if (sync.bound && !sync.cleared) {
    tempPasswordsChanged = await jzPullTempPasswordsIfDue(source === 'resume');
  }
  if (source === 'resume' && sync.bound && !sync.cleared) {
    try {
      const list = await jzFetchUsers({ network: true });
      if (list.length) {
        sync.usersChanged = true;
        jzNotifyUsersChanged();
      }
    } catch {
      /* ignore */
    }
  }
  const newKey = jzBindStateKey();
  const keyChanged = prevKey !== newKey;
  const presenceChanged = !!sync.presenceChanged;
  const usersChanged = !!sync.usersChanged;
  const shouldNotify =
    keyChanged || presenceChanged || usersChanged || tempPasswordsChanged || (sync.bound && !sync.cleared);
  if (shouldNotify) {
    jzLastBindKey = newKey;
    jzFireBindChange({
      bound: !!jzDeviceBindInfo(),
      boundStored: !!jzDeviceBindInfo(),
      cleared: !!sync.cleared,
      lockOnline: sync.lockOnline,
      connectionState: jzConnectionState(),
      presenceChanged,
      usersChanged,
      tempPasswordsChanged,
      source,
      prevKey,
      newKey,
    });
    return true;
  }
  return false;
}

/** 同步 App 本地绑定与后端；后端无记录则清除过期绑定 */
async function jzSyncDeviceBindStatus() {
  const bind = jzDeviceBindInfo();
  if (!bind) return { bound: false, cleared: false, lockOnline: false };
  const prevOnline = jzLockIsOnline(bind);
  const prevUsersRev = Number(bind.lockUsersUpdatedAt) || 0;
  jzBindSyncInFlight = true;
  jzEmitAppEvent('jz-connection-sync', { syncing: true });
  try {
    const deviceName = bind.deviceName || '1111';
    const data = await jzFetchJson(`${jzApiBase()}/devices/bind?deviceName=${encodeURIComponent(deviceName)}`);
    if (data.ok && !data.bound) {
      jzClearDeviceRemoteCaches();
      return { bound: false, cleared: true, lockOnline: false, usersChanged: false };
    }
    const lockOnline = data.lockOnline === true;
    const lockUsersUpdatedAt = Number(data.lockUsersUpdatedAt) || 0;
    let usersChanged = false;
    if (data.bound) {
      const nextBind = {
        ...bind,
        lockOnline,
        lockLastSeenAt: data.lockLastSeenAt || null,
        lockLwtOffline: data.lwtOffline === true,
        lockUsersUpdatedAt,
      };
      jzSaveSettings({ deviceBind: nextBind });
      jzLastPresenceUiKey = '';
      const nextOnline = jzLockIsOnline(nextBind);
      if (lockUsersUpdatedAt > prevUsersRev) {
        usersChanged = await jzPullUsersFromBackend();
      }
      return {
        bound: true,
        cleared: false,
        lockOnline: nextOnline,
        presenceChanged: prevOnline !== nextOnline,
        usersChanged,
      };
    }
    return {
      bound: !!data.bound,
      cleared: false,
      lockOnline,
      presenceChanged: prevOnline !== lockOnline,
      usersChanged,
    };
  } catch {
    return {
      bound: true,
      cleared: false,
      lockOnline: jzLockIsOnline(bind),
      offline: true,
      usersChanged: false,
    };
  } finally {
    jzBindSyncInFlight = false;
    jzEmitAppEvent('jz-connection-sync', { syncing: false, state: jzConnectionState() });
  }
}

async function jzFetchUnlockRecords(opts = {}) {
  const bind = jzDeviceBindInfo();
  const deviceName = opts.deviceName || bind?.deviceName || '1111';
  const months = opts.months || JZ_RECORD_MONTHS;
  const refresh = opts.refresh ? '&refresh=1' : '';
  const timeoutMs = opts.refresh ? 25000 : 4500;
  try {
    const data = await jzFetchJson(
      `${jzApiBase()}/records?deviceName=${encodeURIComponent(deviceName)}&months=${months}${refresh}`,
      {},
      timeoutMs
    );
    if (data.ok && Array.isArray(data.records)) {
      jzSaveSettings({ unlockRecordsCache: { at: Date.now(), deviceName, records: data.records } });
      return data.records;
    }
  } catch {
    /* 离线用缓存 */
  }
  return jzUnlockRecordsFromCache();
}

async function jzFetchUnlockRecordById(id) {
  const cached = jzUnlockRecordsFromCache().find((r) => r.id === id);
  if (cached) return cached;
  try {
    const data = await jzFetchJson(`${jzApiBase()}/records/${encodeURIComponent(id)}`);
    if (data.ok && data.record) return data.record;
  } catch {
    /* ignore */
  }
  return null;
}

function jzTodayUnlockCountFromCache() {
  const start = new Date();
  start.setHours(0, 0, 0, 0);
  const since = start.getTime();
  return jzUnlockRecordsFromCache().filter((r) => r.ts >= since && r.ok !== false).length;
}

async function jzFetchTodayUnlockCount() {
  const bind = jzDeviceBindInfo();
  const deviceName = bind?.deviceName || '1111';
  try {
    const data = await jzFetchJson(`${jzApiBase()}/records/stats/today?deviceName=${encodeURIComponent(deviceName)}`);
    if (data.ok) return data.count;
  } catch {
    /* ignore */
  }
  return jzTodayUnlockCountFromCache();
}

async function jzUnbindDevice() {
  try {
    await fetch(`${jzApiBase()}/devices/bind`, {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ deviceName: jzDeviceBindInfo()?.deviceName }),
    });
  } catch {
    /* 本地解绑仍继续 */
  }
  jzClearDeviceRemoteCaches();
  jzLastBindKey = '';
  jzFireBindChange({ bound: false, cleared: true, source: 'unbind' });
  showToast('App 已解绑；若门锁仍显示已绑定，请在门锁 App配对 页点「重新配对」');
}

/** 是否已设置 6 位远程开锁二次验证密码 */
function jzHasUnlockPin(pin) {
  const p = pin !== undefined ? pin : jzLoadSettings().unlockSecondPin;
  return /^\d{6}$/.test(String(p || '').trim());
}

function jzLoadSettings() {
  try {
    const raw = localStorage.getItem(JZ_SETTINGS_KEY);
    if (!raw) return jzDefaultSettings();
    const merged = { ...jzDefaultSettings(), ...JSON.parse(raw) };
    merged.tempPasswords = jzSanitizeTempPasswords(merged.tempPasswords);
    if (!merged.unlockSecondAuth && merged.unlockSecondPin === '888888') {
      merged.unlockSecondPin = '';
    }
    if (!merged.unlockSecondAuth && merged.unlockSecondPin && !jzHasUnlockPin(merged.unlockSecondPin)) {
      merged.unlockSecondPin = '';
    }
    if (merged._notifyUnreadFix !== 2) {
      merged.notifyUnreadSince = 0;
      merged._notifyUnreadFix = 2;
    }
    return merged;
  } catch {
    return jzDefaultSettings();
  }
}

function jzSaveSettings(partial) {
  const next = { ...jzLoadSettings(), ...partial };
  if (typeof next.avatar === 'number') {
    next.avatar = Math.max(0, Math.min(JZ_AVATARS.length - 1, next.avatar));
  }
  if (next.uiMode && !JZ_UI_MODES[next.uiMode]) next.uiMode = 'standard';
  localStorage.setItem(JZ_SETTINGS_KEY, JSON.stringify(next));
  jzApplyUiMode(next.uiMode);
  return next;
}

function jzUiModeLabel(mode) {
  return (JZ_UI_MODES[mode] || JZ_UI_MODES.standard).label;
}

function jzApplyUiMode(mode) {
  const m = mode || jzLoadSettings().uiMode;
  document.documentElement.setAttribute('data-jz-mode', m);
  const shell = document.querySelector('[data-jz-shell]');
  if (shell) {
    shell.classList.remove('jz-mode-standard', 'jz-mode-adult', 'jz-mode-senior');
    shell.classList.add(`jz-mode-${m}`);
  }
}

function renderAvatar(sizeClass = 'w-10 h-10', extraClass = '') {
  const s = jzLoadSettings();
  const a = JZ_AVATARS[s.avatar] || JZ_AVATARS[0];
  const cls = [
    sizeClass,
    'rounded-full aspect-square overflow-hidden flex-shrink-0',
    'bg-gradient-to-br', a.bg,
    'flex items-center justify-center text-white shadow-sm',
    extraClass,
  ].filter(Boolean).join(' ');
  return `<div class="${cls}" data-jz-avatar>
    <i class="fa-solid ${a.icon} text-[55%]"></i></div>`;
}

function renderAvatarLink(href, sizeClass = 'w-10 h-10', extraClass = '', from) {
  const target = from ? jzHref(href, from) : href;
  return `<a href="${target}" data-jz-avatar-link class="rounded-full inline-block flex-shrink-0">${renderAvatar(sizeClass, extraClass)}</a>`;
}

function renderAvatarPickerGrid(selected) {
  return `<div class="grid grid-cols-3 gap-6 justify-items-center py-2">
    ${JZ_AVATARS.map((a, i) => `
      <button type="button" data-avatar="${i}" class="avatar-pick flex flex-col items-center gap-2 bg-transparent border-0 p-0">
        <div class="avatar-circle w-[4.5rem] h-[4.5rem] rounded-full aspect-square bg-gradient-to-br ${a.bg} flex items-center justify-center text-white text-2xl ring-4 ${i === selected ? 'ring-ycu-teal shadow-card scale-105' : 'ring-slate-100'} transition-all">
          <i class="fa-solid ${a.icon}"></i>
        </div>
        <span class="text-[10px] text-slate-500">头像 ${i + 1}</span>
      </button>`).join('')}
  </div>`;
}

/** 页面主说明（短句，避免挤成参差多行） */
function renderPageDesc(text) {
  return `<p class="jz-page-desc text-sm text-slate-500">${text}</p>`;
}

/** 按钮/卡片下方小字提示 */
function renderHint(text, center = false) {
  return `<p class="jz-hint text-xs text-slate-400 mt-2 ${center ? 'text-center' : ''}">${text}</p>`;
}

/** 列表区块标题 */
function renderSectionLabel(text) {
  return `<p class="jz-section-label text-sm font-medium text-slate-500 px-0.5">${text}</p>`;
}

function renderSwitchRow(id, title, sub, checked) {
  return `
    <div class="flex items-center gap-3 px-4 py-3.5 bg-white">
      <div class="flex-1 min-w-0 pr-2">
        <p class="text-sm font-medium text-ycu-navy">${title}</p>
        ${sub ? `<p class="text-xs text-slate-400 mt-0.5 leading-snug line-clamp-2">${sub}</p>` : ''}
      </div>
      <button type="button" id="${id}" role="switch" aria-checked="${checked ? 'true' : 'false'}"
        class="jz-switch relative w-11 h-6 rounded-full transition-colors ${checked ? 'bg-ycu-teal-dark' : 'bg-slate-200'}">
        <span class="absolute top-0.5 left-0.5 w-5 h-5 rounded-full bg-white shadow transition-transform ${checked ? 'translate-x-5' : ''}"></span>
      </button>
    </div>`;
}

function jzBindSwitch(btnId, getVal, setVal, onChange) {
  const btn = document.getElementById(btnId);
  if (!btn) return;
  function paint(on) {
    btn.setAttribute('aria-checked', on ? 'true' : 'false');
    btn.classList.toggle('bg-ycu-teal-dark', on);
    btn.classList.toggle('bg-slate-200', !on);
    btn.querySelector('span').classList.toggle('translate-x-5', on);
  }
  paint(getVal());
  btn.onclick = () => {
    const next = !getVal();
    setVal(next);
    paint(next);
    if (onChange) onChange(next);
  };
}

function jzShowPinPrompt(opts, onOk, onCancel) {
  const title = opts.title || '二次验证';
  const hint = opts.hint || '输入6位开锁密码';
  const okLabel = opts.okLabel || '确认';
  const shell = document.querySelector('[data-jz-shell]');
  const host = shell || document.body;
  if (shell && getComputedStyle(shell).position === 'static') {
    shell.classList.add('relative');
  }
  const overlay = document.createElement('div');
  overlay.className = `${shell ? 'absolute' : 'fixed'} inset-0 z-[10000] flex items-center justify-center bg-black/45 p-4`;
  overlay.innerHTML = `
    <div class="bg-white rounded-2xl w-full max-w-xs p-5 shadow-card-lg jz-auth-modal">
      <h3 class="text-lg font-bold text-ycu-navy text-center">${title}</h3>
      <p class="jz-modal-line text-slate-500 mt-1 mb-4">${hint}</p>
      <input id="jzAuthInput" type="password" maxlength="6" inputmode="numeric" pattern="[0-9]*" autocomplete="off" placeholder="请输入 6 位数字"
        class="w-full text-center text-2xl tracking-[0.4em] py-3 rounded-xl border-2 border-ycu-teal/30 focus:border-ycu-teal outline-none font-mono"/>
      <p id="jzAuthErr" class="text-xs text-ycu-sun text-center mt-2 h-4"></p>
      <div class="flex gap-2 mt-3">
        <button type="button" id="jzAuthCancel" class="flex-1 py-3 rounded-xl border border-slate-200 text-slate-500 text-sm font-medium">取消</button>
        <button type="button" id="jzAuthOk" class="flex-1 py-3 rounded-xl bg-ycu-teal-dark text-white text-sm font-semibold">${okLabel}</button>
      </div>
    </div>`;
  host.appendChild(overlay);
  const input = document.getElementById('jzAuthInput');
  const err = document.getElementById('jzAuthErr');
  function close() { overlay.remove(); }
  function verify() {
    const val = input.value.trim();
    if (!/^\d{6}$/.test(val)) {
      err.textContent = '请输入6位数字';
      input.classList.add('border-ycu-sun');
      return;
    }
    close();
    if (onOk) onOk(val);
  }
  document.getElementById('jzAuthOk').onclick = verify;
  document.getElementById('jzAuthCancel').onclick = () => { close(); if (onCancel) onCancel(); };
  overlay.addEventListener('click', (e) => { if (e.target === overlay) { close(); if (onCancel) onCancel(); } });
  input.addEventListener('keydown', (e) => { if (e.key === 'Enter') verify(); });
  input.focus();
}

function jzShowSecondAuth(onOk, onCancel) {
  const s = jzLoadSettings();
  if (!s.unlockSecondAuth || !jzHasUnlockPin(s.unlockSecondPin)) {
    showToast('请先在开锁设置开启验证', 'error');
    if (onCancel) onCancel();
    return;
  }
  jzShowPinPrompt({
    title: '二次验证',
    hint: '输入6位开锁密码',
    okLabel: '确认开锁',
  }, (val) => {
    if (val === s.unlockSecondPin) {
      if (onOk) onOk();
    } else {
      showToast('密码错误，请重试', 'error');
      if (onCancel) onCancel();
    }
  }, onCancel);
}

function jzGenerateTempPassword() {
  const taken = jzTakenLockAccounts();
  let account = '';
  for (let i = 0; i < 48; i++) {
    const cand = jzRandomDigits(JZ_CRED.TEMP_ACC_LEN, true);
    if (!taken.has(cand)) {
      account = cand;
      break;
    }
  }
  if (!account) account = jzRandomDigits(JZ_CRED.TEMP_ACC_LEN, true);
  const password = jzRandomDigits(JZ_CRED.TEMP_PWD_LEN);
  const now = Date.now();
  return {
    id: 'tp' + now,
    account,
    password,
    isAdmin: false,
    active: true,
    createdAt: now,
    expiresAt: now + JZ_CRED.TEMP_VALID_MS,
    used: false,
  };
}

function jzPruneTempPasswords(list) {
  const now = Date.now();
  return jzSanitizeTempPasswords(list).filter((t) => !t.used && t.expiresAt > now);
}

function jzFormatUserCredHint() {
  return '账号和密码请使用数字';
}

function jzFormatCountdown(ms) {
  const sec = Math.max(0, Math.floor(ms / 1000));
  const m = String(Math.floor(sec / 60)).padStart(2, '0');
  const s = String(sec % 60).padStart(2, '0');
  return `${m}:${s}`;
}

function jzCopyText(text) {
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(text).then(() => showToast('已复制到剪贴板', 'info')).catch(() => showToast(text, 'info'));
  } else {
    showToast(text, 'info');
  }
}

function renderBreadcrumb(crumbs) {
  return `<nav class="px-4 py-2 text-[11px] text-slate-400 flex flex-wrap items-center gap-1 flex-shrink-0 bg-white/80 border-b border-slate-50">
    ${crumbs.map((c, i) => {
      const last = i === crumbs.length - 1;
      const seg = c.href && !last
        ? `<a href="${c.href}" class="text-ycu-teal-dark hover:underline">${c.label}</a>`
        : `<span class="${last ? 'text-ycu-navy font-medium' : ''}">${c.label}</span>`;
      const sep = last ? '' : '<i class="fa-solid fa-chevron-right text-[8px] opacity-40"></i>';
      return seg + sep;
    }).join('')}
  </nav>`;
}

const JZ_COLORS = {
  teal: '#3ECFB4',
  tealDark: '#2DB8A8',
  tealLight: '#E8FAF7',
  mint: '#B8EDE4',
  blue: '#5B9BD5',
  gold: '#FFC857',
  sun: '#FF8C5A',
  navy: '#334155',
  surface: '#F8FCFB',
  card: '#FFFFFF',
};

function jzTailwindConfig() {
  return {
    theme: {
      extend: {
        fontFamily: { sans: ['"Noto Sans SC"', 'Roboto', 'system-ui', 'sans-serif'] },
        colors: {
          ycu: {
            teal: JZ_COLORS.teal,
            'teal-dark': JZ_COLORS.tealDark,
            'teal-light': JZ_COLORS.tealLight,
            mint: JZ_COLORS.mint,
            blue: JZ_COLORS.blue,
            gold: JZ_COLORS.gold,
            sun: JZ_COLORS.sun,
            navy: JZ_COLORS.navy,
            surface: JZ_COLORS.surface,
          },
        },
        boxShadow: {
          'card': '0 2px 12px rgba(46,184,168,.08), 0 1px 3px rgba(15,23,42,.04)',
          'card-lg': '0 8px 28px rgba(46,184,168,.12), 0 2px 8px rgba(15,23,42,.06)',
        },
      },
    },
  };
}

/** 浏览器原型预览用假状态栏；真机 APK 由系统显示，不再重复 */
function renderStatusBar(light = false) {
  if (jzIsNativeApp()) return '';
  const cls = light ? 'text-slate-600' : 'text-slate-600';
  const now = new Date();
  const h = String(now.getHours()).padStart(2, '0');
  const m = String(now.getMinutes()).padStart(2, '0');
  return `
    <div class="h-7 px-5 flex items-center justify-between text-[11px] font-medium ${cls} flex-shrink-0">
      <span>${h}:${m}</span>
      <div class="flex items-center gap-1.5">
        <i class="fa-solid fa-signal text-[9px] text-ycu-teal"></i>
        <i class="fa-solid fa-wifi text-[9px] text-ycu-teal"></i>
      </div>
    </div>`;
}

/** 底部 Tab 根页顶栏：大标题、左对齐、留白充足（记录 / 设备等） */
function renderTabTopBar(title, opts = {}) {
  const { actionIcon = null, actionHref = null, light = true } = opts;
  const bg = 'bg-gradient-to-b from-ycu-teal-light/80 to-white border-b border-ycu-teal/10';
  const btnCls = 'text-ycu-teal hover:bg-ycu-teal/10';
  return `
    <header class="${bg} flex-shrink-0">
      ${renderStatusBar(light)}
      <div class="px-5 pt-2 pb-5 flex items-center justify-between gap-3 min-h-[56px]">
        <h1 class="text-[22px] font-bold text-ycu-navy leading-snug tracking-tight">${title}</h1>
        ${actionHref ? `<a href="${actionHref}" class="w-11 h-11 rounded-full flex items-center justify-center flex-shrink-0 ${btnCls}" aria-label="更多"><i class="fa-solid ${actionIcon || 'fa-ellipsis'} text-lg"></i></a>` : ''}
      </div>
    </header>`;
}

/** NFC 录入示意动画（不依赖外链图） */
function renderNfcEnrollAnimation() {
  return `
    <div class="relative w-full h-36 rounded-2xl bg-gradient-to-br from-ycu-teal-light via-white to-ycu-mint/40 border border-ycu-teal/15 overflow-hidden mb-4" aria-hidden="true">
      <div class="absolute inset-0 flex items-center justify-center gap-6 px-6">
        <div class="jz-nfc-anim-card relative z-10">
          <div class="w-[72px] h-[46px] rounded-lg bg-gradient-to-br from-ycu-blue to-indigo-500 shadow-card flex flex-col justify-between p-2 text-white">
            <i class="fa-solid fa-nfc-symbol text-sm opacity-90"></i>
            <span class="text-[8px] font-mono tracking-wider opacity-80">NFC</span>
          </div>
        </div>
        <div class="relative flex flex-col items-center">
          <span class="jz-nfc-anim-wave absolute w-16 h-16 rounded-full border-2 border-ycu-teal/40"></span>
          <span class="jz-nfc-anim-wave absolute w-16 h-16 rounded-full border-2 border-ycu-teal/25" style="animation-delay:.6s"></span>
          <div class="jz-nfc-anim-reader w-[76px] h-[76px] rounded-2xl bg-white border-2 border-ycu-teal/30 flex items-center justify-center shadow-card relative z-10">
            <i class="fa-solid fa-wifi text-2xl text-ycu-teal-dark rotate-90"></i>
          </div>
          <p class="text-[10px] text-ycu-teal-dark mt-2 font-medium">读卡区</p>
        </div>
      </div>
      <p class="absolute bottom-2 left-0 right-0 text-center text-[10px] text-slate-400">将卡片贴近读卡器</p>
    </div>`;
}

/** 指纹录入示意动画 */
function renderFpEnrollAnimation() {
  return `
    <div class="relative w-32 h-32 mx-auto mb-4">
      <span class="absolute inset-2 rounded-full border-4 border-ycu-blue/25 animate-ping"></span>
      <span class="absolute inset-0 rounded-full border-2 border-ycu-teal/20 jz-nfc-anim-wave"></span>
      <div class="relative w-full h-full rounded-full bg-gradient-to-br from-ycu-teal-light to-white flex items-center justify-center shadow-card border border-ycu-teal/20">
        <i class="fa-solid fa-fingerprint text-5xl text-ycu-teal-dark"></i>
      </div>
    </div>`;
}

/** 录入步骤列表（带循环高亮） */
function renderEnrollSteps(steps, color = 'ycu-teal') {
  const dotBg = color === 'ycu-blue' ? 'bg-ycu-blue' : 'bg-ycu-teal';
  return `
    <ol class="text-left text-sm text-slate-600 space-y-2.5 mb-4">
      ${steps.map((text, i) => `
        <li class="flex gap-2.5 items-start">
          <span class="jz-enroll-step-dot w-6 h-6 rounded-full ${dotBg} text-white text-xs flex items-center justify-center flex-shrink-0 font-semibold" data-active="${i + 1}">${i + 1}</span>
          <span class="pt-0.5 leading-snug">${text}</span>
        </li>`).join('')}
    </ol>`;
}

/** 清新浅色顶栏；back 传 false 时不显示返回 */
function renderTopAppBar(title, opts = {}) {
  const {
    back = 'home.html',
    actionIcon = null,
    actionHref = null,
    light = true,
  } = opts;
  const showBack = back !== false && back !== null && back !== '';
  const bg = light
    ? 'bg-gradient-to-r from-ycu-teal-light via-white to-ycu-mint/30 border-b border-ycu-teal/10'
    : 'bg-ycu-teal text-white';
  const titleCls = light ? 'text-ycu-navy' : 'text-white';
  const btnCls = light ? 'text-ycu-teal hover:bg-ycu-teal/10' : 'text-white hover:bg-white/10';
  const toolbar = showBack
    ? `<div class="h-13 px-3 flex items-center gap-2 pb-2">
        <a href="${back}" class="w-10 h-10 rounded-full flex items-center justify-center flex-shrink-0 ${btnCls}" aria-label="返回"><i class="fa-solid fa-arrow-left"></i></a>
        <h1 class="flex-1 text-[17px] font-semibold ${titleCls} truncate">${title}</h1>
        ${actionHref ? `<a href="${actionHref}" class="w-10 h-10 rounded-full flex items-center justify-center flex-shrink-0 ${btnCls}"><i class="fa-solid ${actionIcon || 'fa-ellipsis'}"></i></a>` : '<span class="w-10 flex-shrink-0"></span>'}
      </div>`
    : `<div class="h-13 px-5 flex items-center justify-between pb-2">
        <h1 class="text-[17px] font-semibold ${titleCls}">${title}</h1>
        ${actionHref ? `<a href="${actionHref}" class="w-10 h-10 rounded-full flex items-center justify-center flex-shrink-0 ${btnCls}"><i class="fa-solid ${actionIcon || 'fa-ellipsis'}"></i></a>` : ''}
      </div>`;
  return `
    <header class="${bg} flex-shrink-0 shadow-card">
      ${renderStatusBar(light)}
      ${toolbar}
    </header>`;
}

function renderBottomNav(active = 'home') {
  const tabs = [
    { id: 'home', icon: 'fa-house', label: '首页', href: 'home.html' },
    { id: 'records', icon: 'fa-clock-rotate-left', label: '记录', href: 'records.html' },
    { id: 'devices', icon: 'fa-door-closed', label: '设备', href: 'devices.html' },
    { id: 'profile', icon: 'fa-circle-user', label: '我的', href: 'profile.html' },
  ];
  return `
    <nav class="jz-tab-nav h-[60px] bg-white border-t border-slate-200/80 flex items-stretch flex-shrink-0">
      ${tabs.map((t) => {
        const on = active === t.id;
        return `
          <a href="${t.href}" class="relative flex-1 flex flex-col items-center justify-center gap-1 min-w-0 pt-2 pb-2 ${on ? 'text-ycu-teal' : 'text-slate-400'}" ${on ? 'aria-current="page"' : ''}>
            <span class="jz-tab-line absolute top-0 left-1/2 -translate-x-1/2 h-[3px] rounded-full ${on ? 'w-10 opacity-100' : 'w-0 opacity-0'}" aria-hidden="true"></span>
            <i class="fa-solid ${t.icon} text-[20px] leading-none"></i>
            <span class="text-[10px] leading-none ${on ? 'font-semibold' : 'font-medium'}">${t.label}</span>
          </a>`;
      }).join('')}
    </nav>`;
}

/** 首页设备条：已绑定即显示（在线 / 离线） */
function renderDeviceStrip(href) {
  const bind = jzDeviceBindInfo();
  if (!bind) return '';
  const meta = jzDeviceConnectionMeta();
  const target = href || jzHref('device_detail.html', 'home');
  return `
    <a href="${target}" class="mx-4 mb-3 flex items-center gap-3 p-3 rounded-2xl bg-white shadow-card border border-ycu-teal/10 active:scale-[0.99] transition-transform">
      ${renderLockImage('w-14 h-14 flex-shrink-0 rounded-2xl overflow-hidden ring-1 ring-ycu-teal/20')}
      <div class="flex-1 min-w-0">
        <div class="flex items-center gap-1.5 text-[11px] ${meta.textClass} font-medium">
          <span class="w-1.5 h-1.5 rounded-full ${meta.dotClass}"></span>${meta.stripLine}
        </div>
        <p class="font-semibold text-ycu-navy text-[15px] truncate mt-0.5">${bind.deviceLabel || '智能门锁'}</p>
      </div>
      <i class="fa-solid fa-chevron-right text-slate-300 text-sm"></i>
    </a>`;
}

/** 设备管理页主卡片：已绑定即显示 */
function renderDeviceCardLarge(href) {
  const bind = jzDeviceBindInfo();
  if (!bind) return '';
  const meta = jzDeviceConnectionMeta();
  const target = href || jzHref('device_detail.html', 'devices');
  return `
    <a href="${target}" class="block bg-white rounded-2xl overflow-hidden shadow-card-lg border border-ycu-teal/10 active:scale-[0.99] transition-transform">
      ${renderLockImage('w-full h-40')}
      <div class="p-4 flex items-start justify-between gap-3">
        <div class="min-w-0">
          <h2 class="font-bold text-ycu-navy text-[15px] truncate">${bind.deviceLabel || '智能门锁'}</h2>
          <p class="text-xs text-slate-400 mt-1">${meta.stripLine}</p>
        </div>
        <span class="px-2.5 py-1 rounded-lg text-xs font-semibold border flex-shrink-0 ${meta.badgeClass}">${meta.badgeLabel}</span>
      </div>
    </a>`;
}

function renderDevicesEmptyHint() {
  return `
    <div class="rounded-2xl border border-dashed border-slate-200 bg-white/70 px-4 py-10 text-center">
      <i class="fa-solid fa-door-closed text-3xl text-slate-300 mb-3"></i>
      <p class="text-sm text-ycu-navy font-medium">暂无已绑定设备</p>
      ${renderHint('点击下方「添加设备」，输入门锁 6 位配对码即可绑定', true)}
    </div>`;
}

/** 设置列表行（href 为空或 # 时为只读行） */
function renderSettingsGroup(items) {
  const rowInner = (icon, title, sub, badge, linkable, subId) => `
    <div class="w-9 h-9 rounded-xl bg-ycu-teal-light flex items-center justify-center flex-shrink-0">
      <i class="fa-solid ${icon} text-ycu-teal-dark text-sm"></i>
    </div>
    <div class="flex-1 min-w-0">
      <p class="text-sm font-medium text-ycu-navy">${title}</p>
      ${sub ? `<p ${subId ? `id="${subId}" ` : ''}class="text-xs text-slate-400 mt-0.5 leading-snug line-clamp-2">${sub}</p>` : ''}
    </div>
    ${badge ? `<span class="text-[10px] px-2 py-0.5 rounded-full bg-ycu-sun/15 text-ycu-sun font-medium">${badge}</span>` : ''}
    ${linkable ? '<i class="fa-solid fa-chevron-right text-slate-300 text-xs"></i>' : ''}`;

  return `
    <div class="bg-white rounded-2xl shadow-card border border-slate-100/80 overflow-hidden divide-y divide-slate-50">
      ${items.map((row) => {
        const [href, icon, title, sub, badge, subId] = row;
        const linkable = href && href !== '#';
        const cls = 'flex items-center gap-3 px-4 py-3.5';
        if (!linkable) {
          return `<div class="${cls}">${rowInner(icon, title, sub, badge, false, subId)}</div>`;
        }
        return `<a href="${href}" class="${cls} active:bg-ycu-teal-light/50 transition-colors">${rowInner(icon, title, sub, badge, true, subId)}</a>`;
      }).join('')}
    </div>`;
}

function jzNotifyIsEnabled(item, s) {
  if (!s.notifyPush) return false;
  if (item.cat === 'unlock') return s.notifyUnlock;
  if (item.cat === 'alert') return s.notifyAlert;
  if (item.cat === 'device') return s.notifyDevice;
  if (item.cat === 'system') return s.notifySystem;
  return true;
}

function jzNotifyFromCache() {
  const cache = jzLoadSettings().notifyCache;
  return cache && Array.isArray(cache.items) ? cache.items : [];
}

function jzEmitNotifyChanged() {
  jzEmitAppEvent('jz-notify-changed', { unread: jzNotifyUnreadCount() });
}

function jzPaintNotifyBadges() {
  const unread = jzNotifyUnreadCount();
  const stat = document.getElementById('homeNotifyStatCount');
  const bellWrap = document.getElementById('homeNotifyBellBadge');
  if (stat) stat.textContent = String(unread);
  if (bellWrap) {
    if (unread > 0) {
      bellWrap.innerHTML = `<span class="absolute -top-0.5 -right-0.5 min-w-[1rem] h-4 px-0.5 bg-ycu-sun text-white text-[9px] rounded-full flex items-center justify-center font-bold">${unread}</span>`;
      bellWrap.classList.remove('hidden');
    } else {
      bellWrap.innerHTML = '';
      bellWrap.classList.add('hidden');
    }
  }
  const profileBadge = document.getElementById('profileNotifyBadge');
  if (profileBadge) {
    profileBadge.textContent = unread ? `${unread}条未读` : '开锁提醒、告警推送';
  }
  return unread;
}

async function jzFetchNotifications(opts = {}) {
  const bind = jzDeviceBindInfo();
  const deviceName = opts.deviceName || bind?.deviceName || '1111';
  const months = opts.months || JZ_RECORD_MONTHS;
  const syncStartedAt = Date.now();
  try {
    const data = await jzFetchJson(`${jzApiBase()}/notifications?deviceName=${encodeURIComponent(deviceName)}&months=${months}`);
    if (data.ok && Array.isArray(data.notifications)) {
      const items = data.notifications.map((n) => ({
        ...n,
        href: `notification_detail.html?id=${encodeURIComponent(n.id)}`,
      }));
      jzSaveSettings({ notifyCache: { at: syncStartedAt, deviceName, items } });
      jzEmitNotifyChanged();
      return items;
    }
  } catch {
    /* 离线 */
  }
  return jzNotifyFromCache();
}

function jzNotifyVisibleItems() {
  const s = jzLoadSettings();
  return jzNotifyFromCache().filter((n) => jzNotifyIsEnabled(n, s));
}

function jzNotifyIsUnread(item, s) {
  const settings = s || jzLoadSettings();
  const read = new Set(settings.notifyReadIds || []);
  if (read.has(item.id)) return false;
  const since = Number(settings.notifyUnreadSince) || 0;
  if (since > 0 && (Number(item.ts) || 0) <= since) return false;
  return true;
}

function jzNotifyUnreadCount() {
  const s = jzLoadSettings();
  return jzNotifyVisibleItems().filter((n) => jzNotifyIsUnread(n, s)).length;
}

/** 通知列表项 */
function renderNotifyItem(item, index) {
  const { href, title, desc, time, icon, color, unread } = item;
  return `
    <a href="${href}" class="flex gap-3 px-4 py-3.5 ${index > 0 ? 'border-t border-slate-50' : ''} ${unread ? 'bg-gradient-to-r from-ycu-teal-light/50 to-white' : 'bg-white'} active:bg-ycu-mint/30">
      <div class="w-11 h-11 rounded-2xl ${color} flex items-center justify-center flex-shrink-0 shadow-sm">
        <i class="fa-solid ${icon} text-base"></i>
      </div>
      <div class="flex-1 min-w-0">
        <div class="flex items-start justify-between gap-3">
          <p class="font-semibold text-sm text-ycu-navy leading-tight">${title}</p>
          <span class="text-xs text-slate-400 flex-shrink-0 tabular-nums">${time}</span>
        </div>
        <p class="text-sm text-slate-500 mt-1 leading-snug line-clamp-2">${desc}</p>
      </div>
      <div class="flex-shrink-0 w-4 flex items-center justify-center self-center">
        ${unread ? '<span class="w-2 h-2 rounded-full bg-ycu-teal"></span>' : '<i class="fa-solid fa-chevron-right text-slate-200 text-xs"></i>'}
      </div>
    </a>`;
}

function initAndroidPhone(innerHtml, opts = {}) {
  jzEnsureBase();
  const { tab = null, bar = null, bg = 'bg-ycu-surface', fullBleed = false, noShell = false } = opts;
  const body = fullBleed
    ? innerHtml
    : `${bar || ''}<main class="flex-1 flex flex-col overflow-hidden ${bg}">${innerHtml}</main>${tab ? renderBottomNav(tab) : ''}`;

  if (noShell) {
    document.body.className = `${bg} min-h-screen`;
    document.body.innerHTML = body;
    return;
  }

  if (jzIsNativeApp()) {
    document.documentElement.classList.add('jz-native');
    document.body.className = `h-full m-0 ${bg}`;
    document.body.innerHTML = `
      <div class="w-full h-full min-h-[100dvh] flex flex-col overflow-hidden ${bg} relative jz-mode-standard" data-jz-shell
           style="padding: env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left);">
        ${body}
      </div>`;
    jzApplyUiMode();
    return;
  }

  document.body.className = 'min-h-screen flex items-center justify-center p-4 bg-gradient-to-br from-ycu-teal-light via-ycu-mint/40 to-white';
  document.body.innerHTML = `
    <div class="w-[390px] h-[844px] bg-slate-800/90 rounded-[32px] p-2 shadow-card-lg">
      <div class="w-full h-full rounded-[26px] overflow-hidden flex flex-col bg-ycu-surface relative jz-mode-standard" data-jz-shell>
        ${body}
      </div>
    </div>`;
  jzApplyUiMode();
}

function bindLongPress(btnId, fillId, duration = 2000, onComplete) {
  bindUnlockLongPress({ btnId, fillId, duration, onComplete });
}

/**
 * 远程开锁长按：外圈 SVG 进度环 + 内圈渐变填充 + 文案/百分比反馈
 */
function bindUnlockLongPress(opts = {}) {
  const {
    btnId = 'holdBtn',
    ringId = 'holdRing',
    fillId = 'holdFill',
    hintId = 'holdHint',
    pctId = 'holdPct',
    iconId = 'holdIcon',
    duration = 2000,
    onComplete,
  } = opts;

  const btn = document.getElementById(btnId);
  const ring = document.getElementById(ringId);
  const fill = document.getElementById(fillId);
  const hint = document.getElementById(hintId);
  const pctEl = document.getElementById(pctId);
  const icon = document.getElementById(iconId);
  if (!btn) return;

  const circumference = ring ? parseFloat(ring.dataset.circ || '0') || 2 * Math.PI * 92 : 0;
  let start = 0;
  let raf = null;
  let holding = false;
  let done = false;

  function setRing(pct) {
    if (!ring || !circumference) return;
    ring.style.strokeDashoffset = String(circumference * (1 - pct / 100));
  }

  function setHint(text) {
    if (hint) hint.textContent = text;
  }

  function setPct(pct) {
    if (pctEl) pctEl.textContent = Math.round(pct) + '%';
  }

  function reset() {
    if (done) return;
    holding = false;
    cancelAnimationFrame(raf);
    setRing(0);
    if (fill) {
      fill.style.height = '0%';
      fill.style.opacity = '0';
    }
    btn.classList.remove('scale-95', 'ring-4', 'ring-ycu-teal/40');
    btn.style.transform = '';
    setHint('长按 2 秒确认开锁');
    setPct(0);
    if (pctEl) pctEl.classList.remove('text-ycu-teal-dark', 'scale-110');
    if (icon) {
      icon.className = 'fa-solid fa-lock text-4xl mb-1 relative z-10 transition-all duration-300';
    }
  }

  function success() {
    done = true;
    holding = false;
    cancelAnimationFrame(raf);
    setRing(100);
    setPct(100);
    if (onComplete) {
      onComplete({
        finishSuccess: () => {
          setHint('开锁成功');
          btn.classList.add('scale-105');
          btn.style.transform = 'scale(1.05)';
          if (icon) icon.className = 'fa-solid fa-lock-open text-4xl mb-1 relative z-10 text-white animate-pulse';
          if (navigator.vibrate) navigator.vibrate([30, 50, 30]);
          setTimeout(resetSuccess, 2200);
        },
        resetHold: () => {
          done = false;
          reset();
        },
      });
      return;
    }
    setHint('开锁成功');
    if (navigator.vibrate) navigator.vibrate([30, 50, 30]);
    setTimeout(resetSuccess, 2200);
  }

  function resetSuccess() {
    done = false;
    reset();
    setHint('长按 2 秒确认开锁');
  }

  function tick() {
    if (!holding || done) return;
    const elapsed = Date.now() - start;
    const pct = Math.min((elapsed / duration) * 100, 100);
    setRing(pct);
    setPct(pct);
    if (fill) {
      fill.style.height = pct + '%';
      fill.style.opacity = String(0.35 + (pct / 100) * 0.45);
    }
    if (pct < 35) setHint('继续按住…');
    else if (pct < 75) setHint('即将开锁…');
    else if (pct < 100) setHint('继续按住…');
    if (pct >= 100) {
      setHint('正在请求门锁…');
      success();
      return;
    }
    raf = requestAnimationFrame(tick);
  }

  function down(e) {
    if (done) return;
    if (e.type === 'touchstart') e.preventDefault();
    holding = true;
    start = Date.now();
    btn.classList.add('scale-95');
    if (pctEl) pctEl.classList.add('text-ycu-teal-dark', 'scale-110');
    setHint('继续按住…');
    tick();
  }

  function up() {
    if (done) return;
    if (holding) reset();
  }

  btn.addEventListener('mousedown', down);
  btn.addEventListener('mouseup', up);
  btn.addEventListener('mouseleave', up);
  btn.addEventListener('touchstart', down, { passive: false });
  btn.addEventListener('touchend', up);
  btn.addEventListener('touchcancel', up);
}

/** 渲染远程开锁长按按钮（外圈进度环 + 内圈渐变） */
function renderUnlockHoldButton() {
  const r = 92;
  const c = 2 * Math.PI * r;
  return `
    <div class="relative flex flex-col items-center">
      <div class="relative w-[200px] h-[200px] flex items-center justify-center">
        <svg class="absolute inset-0 w-full h-full -rotate-90 pointer-events-none" viewBox="0 0 200 200" aria-hidden="true">
          <circle cx="100" cy="100" r="${r}" fill="none" stroke="#E8FAF7" stroke-width="8"/>
          <circle id="holdRing" cx="100" cy="100" r="${r}" fill="none"
            stroke="url(#unlockGrad)" stroke-width="8" stroke-linecap="round"
            stroke-dasharray="${c}" stroke-dashoffset="${c}"
            data-circ="${c}"
            class="transition-[stroke-dashoffset] duration-75 ease-linear drop-shadow-sm"/>
          <defs>
            <linearGradient id="unlockGrad" x1="0%" y1="0%" x2="100%" y2="100%">
              <stop offset="0%" stop-color="#3ECFB4"/>
              <stop offset="50%" stop-color="#2DB8A8"/>
              <stop offset="100%" stop-color="#5B9BD5"/>
            </linearGradient>
          </defs>
        </svg>
        <button id="holdBtn" type="button"
          class="relative z-10 w-[156px] h-[156px] rounded-full bg-gradient-to-br from-ycu-teal via-ycu-teal-dark to-ycu-blue text-white shadow-card-lg flex flex-col items-center justify-center overflow-hidden select-none touch-none transition-transform duration-150 active:scale-95">
          <div id="holdFill" class="absolute bottom-0 left-0 right-0 h-0 bg-gradient-to-t from-white/50 via-ycu-mint/40 to-transparent pointer-events-none transition-none rounded-full" style="opacity:0"></div>
          <div class="absolute inset-0 bg-gradient-to-br from-white/10 to-transparent pointer-events-none rounded-full"></div>
          <i id="holdIcon" class="fa-solid fa-lock text-4xl mb-1 relative z-10 transition-all duration-300"></i>
          <span id="holdPct" class="text-lg font-bold relative z-10 tabular-nums leading-none">0%</span>
        </button>
      </div>
      <p id="holdHint" class="text-sm text-slate-500 mt-5 font-medium transition-colors">长按 2 秒确认开锁</p>
      <p class="text-[11px] text-slate-400 mt-1">松开手指可取消</p>
    </div>`;
}

/** 主 Tab 页弹性滚动 + 下拉刷新（顶栏固定、仅内容区滚动） */
function jzInitTabScroll(container, opts = {}) {
  const {
    onRefresh = () => Promise.resolve(),
    pullingText = '下拉刷新',
    refreshText = '松开刷新',
  } = opts;
  const root = typeof container === 'string' ? document.getElementById(container) : container;
  if (!root) return;

  root.classList.add('jz-tab-scroll');

  let inner = root.querySelector(':scope > .jz-tab-scroll-inner');
  if (!inner) {
    inner = document.createElement('div');
    inner.className = 'jz-tab-scroll-inner';
    while (root.firstChild) inner.appendChild(root.firstChild);
    root.appendChild(inner);
  }

  const indicator = document.createElement('div');
  indicator.className = 'jz-pull-indicator';
  indicator.innerHTML = '<i class="fa-solid fa-arrow-down jz-pull-icon"></i><span class="jz-pull-text ml-1.5"></span>';
  root.prepend(indicator);

  let startY = 0;
  let startX = 0;
  let pullOffset = 0;
  let dragging = false;
  let loading = false;
  const threshold = 68;
  const maxPull = 96;

  function setPull(y) {
    pullOffset = Math.max(0, Math.min(y, maxPull));
    const ready = pullOffset >= threshold;
    indicator.style.height = `${pullOffset}px`;
    inner.style.transform = pullOffset ? `translateY(${pullOffset}px)` : '';
    indicator.querySelector('.jz-pull-text').textContent = ready ? refreshText : pullingText;
    root.classList.toggle('jz-pull-ready', ready);
  }

  function resetPull(animate = true) {
    pullOffset = 0;
    dragging = false;
    if (!animate) inner.style.transition = 'none';
    indicator.style.height = '0';
    inner.style.transform = '';
    root.classList.remove('jz-pull-ready', 'jz-pull-dragging', 'jz-pull-loading');
    if (!animate) requestAnimationFrame(() => { inner.style.transition = ''; });
  }

  function atBottom() {
    return root.scrollTop + root.clientHeight >= root.scrollHeight - 2;
  }

  root.addEventListener('touchstart', (e) => {
    if (loading) return;
    startY = e.touches[0].clientY;
    startX = e.touches[0].clientX;
  }, { passive: true });

  root.addEventListener('touchmove', (e) => {
    if (loading) return;
    const y = e.touches[0].clientY;
    const dy = y - startY;

    if (root.scrollTop <= 0 && dy > 0) {
      dragging = true;
      root.classList.add('jz-pull-dragging');
      setPull(dy * 0.42);
      if (e.cancelable) e.preventDefault();
      return;
    }

    if (dragging) {
      resetPull();
      return;
    }

    if (atBottom() && dy < 0) {
      const over = Math.min(Math.abs(dy) * 0.18, 36);
      root.classList.add('jz-pull-dragging');
      inner.style.transform = `translateY(-${over}px)`;
      if (e.cancelable) e.preventDefault();
    } else if (inner.style.transform && !pullOffset) {
      inner.style.transform = '';
      root.classList.remove('jz-pull-dragging');
    }
  }, { passive: false });

  root.addEventListener('touchend', async () => {
    if (loading) return;
    if (dragging && pullOffset >= threshold) {
      loading = true;
      root.classList.add('jz-pull-loading');
      indicator.querySelector('.jz-pull-text').textContent = '刷新中…';
      indicator.style.height = '52px';
      inner.style.transform = 'translateY(52px)';
      try {
        await onRefresh();
      } finally {
        loading = false;
        resetPull();
      }
      return;
    }
    resetPull();
  }, { passive: true });

  root.addEventListener('touchcancel', () => resetPull(), { passive: true });
}

/** 引导页：跟手滑动 + 松手缓动吸附（类似主流 App 引导轮播） */
function jzInitOnboardingCarousel(opts = {}) {
  const {
    viewport,
    track,
    slideCount,
    getIndex,
    setIndex,
  } = opts;
  if (!viewport || !track || !slideCount) return { goTo() {} };

  let startX = 0;
  let startY = 0;
  let dragging = false;
  let offsetX = 0;

  function slideWidth() {
    return viewport.clientWidth || window.innerWidth;
  }

  function paint(offset = 0, animate = false) {
    track.style.transition = animate
      ? 'transform 0.38s cubic-bezier(0.22, 0.61, 0.36, 1)'
      : 'none';
    const base = -getIndex() * slideWidth();
    track.style.transform = `translate3d(${base + offset}px, 0, 0)`;
  }

  function goTo(index, animate = true) {
    const next = Math.max(0, Math.min(slideCount - 1, index));
    setIndex(next);
    offsetX = 0;
    dragging = false;
    paint(0, animate);
  }

  viewport.addEventListener('touchstart', (e) => {
    startX = e.touches[0].clientX;
    startY = e.touches[0].clientY;
    dragging = false;
    offsetX = 0;
    track.style.transition = 'none';
  }, { passive: true });

  viewport.addEventListener('touchmove', (e) => {
    const x = e.touches[0].clientX;
    const y = e.touches[0].clientY;
    const dx = x - startX;
    const dy = y - startY;
    if (!dragging) {
      if (Math.abs(dx) < 10 || Math.abs(dx) < Math.abs(dy) * 1.15) return;
      dragging = true;
    }
    offsetX = dx;
    const i = getIndex();
    if ((i === 0 && dx > 0) || (i === slideCount - 1 && dx < 0)) {
      offsetX = dx * 0.32;
    }
    paint(offsetX, false);
    if (e.cancelable) e.preventDefault();
  }, { passive: false });

  viewport.addEventListener('touchend', () => {
    if (!dragging) return;
    const threshold = slideWidth() * 0.18;
    let next = getIndex();
    if (offsetX < -threshold) next += 1;
    else if (offsetX > threshold) next -= 1;
    goTo(next, true);
  }, { passive: true });

  viewport.addEventListener('touchcancel', () => goTo(getIndex(), true), { passive: true });
  window.addEventListener('resize', () => paint(0, false));
  paint(0, false);

  return { goTo };
}

function showToast(msg, type = 'success') {
  void type;
  const t = document.createElement('div');
  t.className = 'jz-toast fixed top-24 left-1/2 -translate-x-1/2 z-[9999] px-5 py-3 rounded-2xl bg-ycu-teal-dark text-white text-sm font-medium shadow-card-lg';
  t.textContent = msg;
  document.body.appendChild(t);
  setTimeout(() => t.remove(), 2400);
}

function lockMethodIcon(method) {
  return {
    密码: 'fa-key',
    指纹: 'fa-fingerprint',
    NFC: 'fa-nfc-symbol',
    手机: 'fa-mobile-screen',
    远程: 'fa-mobile-screen',
    临时: 'fa-clock',
  }[method] || 'fa-lock';
}

/** 生成临时密码（走后端下发门锁） */
async function jzCreateTempPassword(opts = {}) {
  const bind = jzDeviceBindInfo();
  const deviceName = opts.deviceName || bind?.deviceName || '1111';
  const reserved = opts.reservedAccounts || jzTakenLockAccounts();
  try {
    const resp = await fetch(`${jzApiBase()}/temp-passwords`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ deviceName, reservedAccounts: [...reserved] }),
    });
    const data = await resp.json().catch(() => ({}));
    if (!resp.ok || !data.ok || !data.tempPassword) {
      showToast(data.message || '生成失败');
      return { ok: false, data };
    }
    await jzPullTempPasswordsIfDue(true);
    return { ok: true, tempPassword: data.tempPassword };
  } catch {
    showToast('无法连接后端');
    return { ok: false };
  }
}

/** 作废临时密码 */
async function jzRevokeTempPassword(id) {
  try {
    const resp = await fetch(`${jzApiBase()}/temp-passwords/${encodeURIComponent(id)}`, {
      method: 'DELETE',
    });
    const data = await resp.json().catch(() => ({}));
    if (!resp.ok || !data.ok) {
      showToast(data.message || '作废失败');
      return { ok: false };
    }
    await jzPullTempPasswordsIfDue(true);
    return { ok: true };
  } catch {
    showToast('无法连接后端');
    return { ok: false };
  }
}

/** 拉取后端有效临时密码列表 */
async function jzFetchTempPasswords() {
  const bind = jzDeviceBindInfo();
  const deviceName = bind?.deviceName || '1111';
  try {
    const data = await jzFetchJson(`${jzApiBase()}/temp-passwords?deviceName=${encodeURIComponent(deviceName)}`);
    if (data.ok && Array.isArray(data.items)) {
      jzSaveSettings({ tempPasswords: data.items });
      jzLastTempPasswordSig = jzTempPasswordListSig(data.items);
      return data.items;
    }
  } catch {
    /* ignore */
  }
  return jzPruneTempPasswords(jzLoadSettings().tempPasswords);
}

/** 通用子页：设置表单布局
 *  opts.backAnchor: 'profile' 固定返回「我的」；'fixed' 固定 back 参数不读 ?from=
 */
function initSettingsPage(title, back, sectionsHtml, opts = {}) {
  jzEnsureBase();
  let resolvedBack;
  if (opts.backAnchor === 'profile') {
    resolvedBack = 'profile.html';
  } else if (opts.backAnchor === 'fixed' && typeof back === 'string') {
    resolvedBack = back;
  } else {
    resolvedBack = typeof back === 'string' ? jzResolveBack(back) : back;
  }
  const tab = opts.tab || null;
  initAndroidPhone(`
    ${renderTopAppBar(title, { back: resolvedBack })}
    <div class="flex-1 overflow-y-auto px-4 py-4 space-y-4">${sectionsHtml}</div>
  `, { fullBleed: true, tab });
}
