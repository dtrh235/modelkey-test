# 第 21 课：ui_v3 WiFi 与状态刷新排障实录

> **阶段**：联网与 UI | **建议课时**：2～3 小时（结合串口日志对照真机）

---

## 上节关联

- 第 16 课：ui_v3 模块划分与导航栈
- 第 14 课：ESP8266 AT 与 MQTT 状态机
- 第 18 课：分层排障 L0～L5

**本课性质**：不是新功能介绍，而是一次**真实联调踩坑记录**——日志里云端已 `ONLINE`，界面却还显示「离线」，进 WiFi 页扫描没反应，这类问题怎么从串口一眼定位根因。

---

## 学习目标

学完本课你能回答：

- 为什么 ui_v3 开了 WiFi 页，CloudTask 却不扫热点？
- `g_app_scr` 和 `ui3_scr_id_t` 不同步会怎样？
- 顶栏「云端 / 离线」为什么 MQTT 连上了还不变？
- NTP 对时后时钟会变、云端徽章却不跟着变，各走哪条路？

---

## 现象回顾（2026-06 联调）

### 正常侧：云端后台其实已通

```
[ALIYUN] init: ESP AT OK modem_ready=1
[ALIYUN] step=CIPSTART … TCP connected
[ALIYUN] MQTT connected (CONNACK OK) -> cloud online
[CLOUD] MQTT online ok
```

说明 **UART2、ESP 供电、MQTT 状态机** 没问题。

### 异常侧：用户看到的 UI

| 现象 | 用户描述 |
|------|----------|
| WiFi 页 | 进页不自动扫、列表空、连不上 |
| 顶栏云端 | 一直「离线」，要**切到别的页再回来**才变「云端」 |
| 时钟 | NTP 对时后**时间会跳变**（正常），但云端状态不一起更新 |

---

## 根因一：进 WiFi 页时 `g_app_scr` 还没切到 11

### 机制

旧 Guider UI 与 CloudTask 用全局变量 `g_app_scr` 判断「当前是否在 WiFi 屏」：

- `g_app_scr == APP_SCR_11` → 允许 `app_wifi_scan_service()` 跑 CWLAP
- `app_wifi_on_scr11()`、`remember_on_wifi_page()` 等也依赖这个值

ui_v3 有自己的 `ui3_scr_id_t`（`UI3_SCR_WIFI`），**若不同步**，后端以为你还在首页，WiFi 业务全被跳过。

### 踩坑代码路径（已修）

原先在 `menu_enter_impl()` 里**先**调 `ui3_wifi_on_enter()`，**后** `ui3_nav_to()` → `ui3_load()` → `ui3_sync_legacy_scr()`。

此时 `g_app_scr` 仍是菜单（`APP_SCR_3`），不是 `APP_SCR_11`：

```
菜单点「WiFi」
  → ui3_wifi_on_enter()     // g_app_scr 还是 3，扫描条件不满足
  → ui3_nav_to(WIFI)
      → ui3_sync_legacy_scr // 这里才变成 APP_SCR_11，已经晚了
```

旧 Guider 的顺序是：**先切屏、再 prepare**（`enter_screen_wifi` → `screen_wifi_prepare_on_enter`）。

### 修复要点

1. `ui3_wifi_on_enter()` 挪到 `ui3_load()` 里，在 `ui3_sync_legacy_scr()` **之后**调用。
2. WiFi 相关判断增加 `app_wifi_on_scr11()`，不只看 `g_app_scr`。

### 相关文件

| 文件 | 作用 |
|------|------|
| `ui_v3/app_ui_v3.c` | `ui3_sync_legacy_scr()` 映射表 |
| `ui_v3/app_ui_v3_screens.c` | 各屏 `build_*` |
| `app_wifi_scan.c` | 扫描服务、`scan_allowed` |
| `main.c` CloudTask | `g_app_scr == APP_SCR_11` 分支 |

---

## 根因二：异步 CWLAP + ESP 硬复位堵住 CloudTask

### 现象

串口停在：

```
[WiFi][stage] cwlap async_start
[WiFi][stage] esp hw reset
```

之后没有 `[ALIYUN]` 心跳——**整个 CloudTask 被阻塞**，MQTT 也停。

### 原因

- 在 `async_start` 里做 **ESP 硬件复位 + 长超时 AT**，占住 CloudTask。
- GuiTask 若同步 `cwlap_teardown_idle(2500)`，还会卡界面。

### 修复要点

1. WiFi 扫描改在 CloudTask 里 **阻塞式** `cloud_aliyun_at_run_cwlap_scan()`（单一任务、可预期）。
2. 模组未就绪时再复位 ESP，并打 `[WiFi][stage]` 日志。
3. teardown 用 `s_scan_teardown_pending` 延迟到下一轮，不在 GuiTask 里 sleep。

### ESP 启动无回显

```
[ALIYUN] after ESP boot rx_bytes=0 (MCU got nothing: check ESP power, PA2/PA3, TX/RX swap)
```

`rx_bytes=0` 表示 MCU  UART2 没收到任何字节，优先查：

- ESP 供电 / EN 脚
- PA2(TX)→ESP RX、PA3(RX)→ESP TX 是否接反
- 波特率 115200

后面若出现 `AT OK modem_ready=1`，说明模组已醒，可继续查 CWLAP。

---

## 根因二点五：开机 AT 偶发失败 + poll 被 defer（只有 CloudTask alive）

> **实测补充**：若 ESP **未插 5V**，会出现 `rx_bytes=18`、乱码、`AT probe fail`、长期只有 `CloudTask alive`——**先插好模组电源再查软件**。下文为软件侧 defer 说明（`ui3-wifi-fix` 行为）。

### 现象

```
[ALIYUN] after ESP boot rx_bytes=18 data="|"
[ALIYUN] init: ESP AT probe fail (retry in poll)
[ALIYUN] cloud AT/MQTT poll deferred until WiFi page or STA up
[RTOS] CloudTask alive
[RTOS] CloudTask alive
…（一直无 step=AT / MQTT）
```

Flash 里已有 WiFi（如 `liu`），但停在首页不动。

### 原因

1. **ESP 启动时序不稳定**：复位后有时只收到十几字节乱码（如 `|`），第一次 `AT` 等不到 `OK`。
2. **`poll_allowed` 过严**：原先要求 `modem_ready=1` 才在首页 poll。探活失败 → 首页**完全不跑状态机**，只能进 WiFi 页才重试——所以「刚才好用、冷启动又不行」往往是**偶发时序**，不是线突然断了。

### 修复要点（ui_v3-wifi-fix）

1. 进 WiFi 页、`on_enter` 在 `g_app_scr` 同步之后（见根因一）。
2. 首页 poll：需 `modem_ready=1` 且 Flash 有 SSID 才自动建 MQTT；探活失败则 **进 WiFi 页** 扫描会走 CWLAP 里的 hw reset 恢复。

### 如何区分

| 日志 | 含义 |
|------|------|
| `rx_bytes=0` | 完全无数据 → **先查 ESP 5V 供电**、PA2/PA3 接线 |
| `rx_bytes=18` 有乱码 | 常见是 **ESP 未上电或供电不足**（用户实测忘插 5V 即此现象），其次才是时序 |
| 只有 `CloudTask alive` | poll 被 defer；有记住 WiFi 且 `modem_ready=1` 后首页才会跑状态机 |

---

### 机制

`ui3_topbar()` 根据 `st->mqtt_online` 创建圆角徽章和「云端 / 离线」文字。  
`ui3_services_sync_cloud()` 在 `app_ui_v3_poll()` 里**会更新变量**，但**不会改已有 LVGL 控件**。

首页加载时 MQTT 往往还没 CONNACK，`mqtt_online=0` → 显示「离线」。  
十几秒后 CloudTask 上线，变量变成 1，**控件仍是离线样式**，直到 `ui3_reload_current()`（换页触发重建）。

这就是「跳转页面才显示云端」。

### 修复要点

1. `ui3_state_t` 保存 `cloud_badge` / `cloud_dot` / `cloud_label` 指针。
2. `mqtt_online` 变化时调 `ui3_topbar_cloud_refresh()`，只改颜色与文字，不整屏 reload。

---

## 根因四：NTP 对时只刷新了旧 Guider 控件

### 机制

`cloud_aliyun_at.c` 对时成功后调用：

- `app_wall_clock_on_set()` — 更新内存里的墙钟（ui_v3 也用这个）
- `app_home_wall_clock_refresh_ui()` — 实现写在 `guider/generated/widgets_init.c`，只更新 `guider_ui.screen_digital_clock_1`

ui_v3 首页大钟、日期、顶栏时间在 `app_ui_v3_tick()` 每秒刷一次，所以 **NTP 后最多 1 秒内钟会跳**——用户感觉「屏幕时间变了」。

但云端徽章不走 tick，仍停留在根因三的问题。

### 修复要点

`app_wall_clock_gui_poll()` 在 `APP_UI_V3_ENABLE=1` 时改调 `ui3_wall_clock_refresh()`，对时后立即刷新 ui_v3 时钟标签；云端走 `ui3_topbar_cloud_refresh()`。

---

## 串口怎么读（本课专用）

| 日志前缀 | 含义 | 正常期望 |
|----------|------|----------|
| `[BOOT]` | 启动阶段 | 到 `before scheduler` 无卡死 |
| `[ALIYUN] step=` | ESP AT/MQTT 状态机 | 最终 `step=ONLINE` |
| `[CLOUD] MQTT online ok` | 业务层认为已上云 | 与顶栏「云端」应对齐（修后无需换页） |
| `[WiFi][stage]` | WiFi 扫描阶段 | 进 WiFi 页应有 `scan_start` / `cwlap ok` |
| `[TIME]Wok` | 墙钟 epoch 合理 | 对时成功 |
| `rx_bytes=0` | ESP 无 UART 回显 | **硬件/接线问题**，先别查 UI |

建议同时打开（`app_config.h`）：

- `APP_WIFI_UART_DEBUG` / `APP_WIFI_CWJAP_TRACE`
- `APP_CLOUD_TRACE`
- `APP_BOOT_STAGE_LOG`

调试口：**USART1 PA9/PA10，115200**。

---

## 任务边界（别在错的线程里改 UI）

```
CloudTask     ESP AT、CWLAP、MQTT、NTP 解析
GuiTask       lv_timer_handler、app_ui_v3_poll/tick、改 LVGL 控件
```

| 可以做 | 不可以做 |
|--------|----------|
| CloudTask 设 `s_home_ui_pending`，GuiTask `app_wall_clock_gui_poll` 刷钟 | CloudTask 里直接 `lv_label_set_text` |
| CloudTask 跑 CWLAP 扫描 | GuiTask 里 `cwlap_teardown_idle` 阻塞 2.5s |
| `ui3_post_feedback_toast` 投递，poll 里弹 | ISR 里重建整屏 |

---

## 与旧 Guider 对照（仅当解释 bug 时）

| 项目 | 旧 Guider | ui_v3（修后） |
|------|-----------|----------------|
| WiFi 进页 | 先 `enter_screen` 再 `prepare` | 先 `ui3_sync_legacy_scr` 再 `ui3_wifi_on_enter` |
| 云端徽章 | Guider 首页另有逻辑 | 顶栏 `ui3_topbar` + 轮询 refresh |
| 对时刷新 | `widgets_init.c` 改 digital_clock | `ui3_wall_clock_refresh` |

**不要**为了「和旧版一样」把扫描挪回 GuiTask 或去掉 `g_app_scr` 同步——那些是 bug 来源。

---

## 本课小结

1. **ui_v3 不是孤岛**：必须维护 `g_app_scr` 与 `ui3_scr` 映射，后端 WiFi/首页 NFC 都靠它。
2. **进页钩子要在导航完成之后**：`on_enter` 早于 `sync_legacy_scr` 会导致 WiFi 永远不扫。
3. **状态变量 ≠ 界面**：`mqtt_online` 变了要 refresh 控件，不能等用户换页。
4. **串口先分 L4/L5**：`MQTT online` 有了还显示离线 → UI 刷新问题；连 `AT OK` 都没有 → ESP/接线问题。

---

## 练习

1. 画出从「菜单点 WiFi」到 `CWLAP` 发出的调用链，标出 `g_app_scr` 何时变为 `APP_SCR_11`。
2. 若日志有 `MQTT online ok` 但顶栏仍离线，应查哪两个函数？
3. `rx_bytes=0` 与 `cwlap rc=-3 no data timeout` 分别说明什么层的问题？

---

## 相关文件索引

| 文件 | 本课关注点 |
|------|------------|
| `ui_v3/app_ui_v3.c` | `ui3_sync_legacy_scr`、`ui3_wall_clock_refresh` |
| `ui_v3/app_ui_v3_services.c` | `ui3_wifi_on_enter`、`ui3_services_sync_cloud` |
| `ui_v3/app_ui_v3_widgets.c` | `ui3_topbar`、`ui3_topbar_cloud_refresh` |
| `app_wifi_scan.c` | 扫描允许条件、teardown |
| `cloud_aliyun_at.c` | MQTT/NTP、禁止在错误上下文阻塞 |
| `app_wall_clock.c` | `app_wall_clock_gui_poll` ui_v3 分支 |
| `app_config.h` | `APP_UI_V3_ENABLE`、`APP_WIFI_AUTO_CONNECT_ENABLE` |
