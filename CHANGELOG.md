# 版本更新说明

本仓库 **Host** 为 STM32F407 门锁主控固件（LVGL 界面 + ESP8266 阿里云 MQTT）；**Client** 为配套从机工程。  
在 GitHub 上可通过 **[Releases](https://github.com/dtrh235/-stm32f407-/releases)** 或下表中的 **标签（Tag）** 下载对应版本源码。

| 版本 | Git 标签 | 提交 | 日期 | 一句话 |
|------|----------|------|------|--------|
| **v1.3.0** | `v1.3.0` | `73dda2e` | 2026-06-01 | 恢复 NFC/指纹、云端解锁时间格式、WiFi 连接 UI、NFC 硬件测试工程 |
| **v1.2.0** | `v1.2.0` | `d94ebe4` | 2026-06-01 | WiFi 扫描/连接稳定性、校时后再上传解锁记录、用户管理调整 |
| **v1.1.0** | `v1.1.0` | `ed081b3` | 2026-06-01 | MQTT 对时（r110 方案）、WiFi 后不再被 SNTP 卡 UI |
| **v1.0.0** | `v1.0.0` | `b6b9d3e` | 首次入库 | Host + Client 完整 Keil 工程基线 |

---

## v1.3.0（当前 main）

**适用场景：** 需要 NFC/指纹联调、阿里云解锁记录带可读时间、单独验证 NFC+RS485 硬件。

### 新增
- **`Host/test/nfc_enroll_rs485/`**：独立 Keil 小工程，仅测 MFRC522 + RS485（USART6），不跑完整 UI；含 `test_preinclude.h` 解决与主工程 `app_config.h` 宏冲突。
- **`Host/test/README.md`**：测试工程索引说明。

### 改进
- **生物识别**：`APP_TEMP_DISABLE_BIOMETRIC=0`，恢复 `FpTask` / `NfcTask` / `HomeAuth` 及首页 NFC 初始化。
- **WiFi 界面**：连接过程显示「正在连接」+ 动画 `...`；扫描/连接省略号不再显示为方框（点阵用 SourceHanSerif，中文用 `lv_font_cn_wifi_25`）；修正文案与省略号布局顺序。
- **云端解锁**：
  - 属性 `unlock_time` 改为字符串 **`YYYY.MM.DD HH:MM`**（与 Flash 队列一致）。
  - 从 **`+IPD`** 解析 `post_reply`，减少「非 200」误报；上传节流（约 10s）；UART 忙时跳过发送。
- **ROM**：Keil 工程剔除未用 LVGL 字体/控件；`lv_conf.h` 精简。

### 配置
- `APP_CLOUD_TRACE`、`APP_TIME_TRACE` 关闭（正式联调日志更少）。
- 主工程 **`APP_RS485_ENABLE` 仍为 0**；RS485 仅在测试工程中开启。

### 主要改动文件
`cloud_aliyun_at.c`、`cloud_ota_service.c`、`app_unlock_flash_queue.c`、`app_screen_wifi_flow.c`、`app_wall_clock.c`、`main.c`、`stm32f407_lvglport.uvprojx`

---

## v1.2.0

**适用场景：** WiFi 反复扫描/连接不稳定、解锁记录在未校时时误上传或丢失。

### 改进
- **WiFi**：`CWLAP` / `CWJAP` 扫描后走快速路径，减少重复 `prep`/空闲探测，提高再次连接成功率。
- **云端解锁**：**未对时前不上传**；记录先入队/Flash，MQTT 在线且时间同步后再批量 `flush`。
- **用户管理**：取消特殊默认管理员槽位；保护「最后一个管理员」不被删光。

### 仓库清理
- 删除 Host/Client 中 **LVGL demos**（benchmark、music、widgets 等）及多余 readme，减小仓库体积。
- 更新 `.gitignore`。

### 调试开关（相对 v1.1.0）
- 关闭 CWJAP 调试跟踪；**开启** `APP_CLOUD_TRACE` / `APP_TIME_TRACE` 便于下一阶段联调（v1.3.0 已再关闭）。

### 主要改动文件
`app_screen_wifi_flow.c`、`app_wifi_scan.c`、`cloud_aliyun_at.c`、`app_unlock_flash_queue.c`、`app_config.h`、大量 demos 删除

---

## v1.1.0

**适用场景：** 连上 WiFi 后界面卡住、云端时间不对、需要与早期 r110 行为一致的 MQTT 对时。

### 改进
- **对时**：恢复 **MQTT 属性/消息侧 NTP（r110 风格）**，与阿里云物模型时间字段配合。
- **体验**：连 WiFi 后 **不再长时间阻塞在 SNTP**，避免 UI 假死。
- **WiFi 流程**：`app_screen_wifi_flow`、`app_wifi_scan`、`app_wifi_diag` 等一轮整理；生成 WiFi 专用中文字体 `lv_font_cn_wifi_25`。
- **云端**：`cloud_aliyun_at`、`cloud_ota_service`、`app_wall_clock`、`app_unlock_flash_queue` 等与会话/OTA 路径调整。
- **界面**：`app_screen7/8/9/10` 弹窗与触摸流程补丁。

### 主要改动文件
`cloud_aliyun_at.c`、`cloud_ota_service.c`、`app_wall_clock.c`、`app_screen_wifi_flow.c`、`main.c`

---

## v1.0.0（基线）

**适用场景：** 从零查看完整双机工程结构。

### 包含内容
- **Host**：STM32F407 + ILI9341 LVGL Guider UI、ESP8266 AT、阿里云 MQTT、NFC（MFRC522）、指纹（AS608）、门锁逻辑、OTA 等。
- **Client**：配套从机 Keil 工程（含 LVGL 中间件，当时仍带 demos）。
- **工程入口**：`Host/Projects/MDK-ARM/stm32f407_lvglport.uvprojx`、`Client/Projects/...`

### 说明
- 此为首次推送到 GitHub 的快照，后续功能以 v1.1.0 起 changelog 为准。

---

## 如何切换版本

```bash
# 查看所有版本标签
git tag -l "v*"

# 切换到某一版（只读查看）
git checkout v1.2.0

# 回到最新 main
git checkout main
```

在 GitHub 网页：**Code → Tags** 或 **Releases** 中选择对应版本。

---

## 版本号规则（本项目约定）

- **主版本**：架构或大改（暂无）
- **次版本**：v1.x.0，每轮可交付联调 milestone 打 tag
- **修订**：同一 milestone 内小修补（可按需 `v1.2.1`）

今后每次推送到 `main` 并准备对外使用时，请同步更新本文件并在 GitHub 创建 **Release + Tag**。
