# modelkey — STM32F407 智能门锁

STM32F407 双机 Keil 工程：**Host（主控 + 屏 + WiFi/云）**、**Client（从机）**，配套 **JoyfulZone** 手机 App 与 Node.js 后端。

| 目录 | 说明 |
|------|------|
| `Host/` | 主控固件：LVGL UI、ESP8266、阿里云 MQTT、NFC、指纹、配对/临时密码、OTA |
| `Client/` | 从机固件（RS485、NFC、临时密码同步） |
| `JoyfulZone/` | 手机 App 原型（HTML）+ Node 后端 + Capacitor 打包 |
| `Host/test/`、`test/` | 硬件/协议单项测试工程 |
| `学习/` | 项目学习笔记与实验（中文） |

## 工程入口

- 主程序 Host：`Host/Projects/MDK-ARM/stm32f407_lvglport.uvprojx`
- 主程序 Client：`Client/Projects/MDK-ARM/stm32f407_lvglport.uvprojx`
- NFC/RS485 测试：`Host/test/nfc_enroll_rs485/MDK-ARM/nfc_enroll_test.uvprojx`

## JoyfulZone（App + 后端）

门锁与手机之间的 MQTT 桥接，经阿里云产品流转与固件 `terminal/push`、`server/get` 互通。

```powershell
# 后端（需先在阿里云创建服务器设备并配置 .env）
cd JoyfulZone/server
copy .env.example .env
npm install
npm start

# App：浏览器打开 JoyfulZone/index.html
# 打包 APK：见 JoyfulZone/mobile/BUILD_APK.md
```

- 后端说明：[JoyfulZone/server/README.md](./JoyfulZone/server/README.md)
- Topic/流转配置：[JoyfulZone/server/TOPIC_SETUP.md](./JoyfulZone/server/TOPIC_SETUP.md)
- App 原型说明：[JoyfulZone/README.md](./JoyfulZone/README.md)

**注意：** `JoyfulZone/server/.env` 含 DeviceSecret，勿提交 Git；复制 `.env.example` 后本地填写。

## 云端数据通路

| 用途 | 通路 |
|------|------|
| App 开锁记录 / 用户同步 / 临时密码 | 门锁 `terminal/push` → 云流转 → 后端 `server/push` |
| 手机远程开锁 / 下发临时密码 | 后端 `server/get` → 云流转 → 门锁 `terminal/get` |
| 阿里云控制台物模型历史 | 门锁 `/sys/.../thing/event/property/post`（与 App 桥接独立） |

## 版本与更新说明

**每个上传到 GitHub 的版本做了什么，请看：**

- **[CHANGELOG.md](./CHANGELOG.md)** — 中文详细更新记录（推荐）
- **[GitHub Releases](https://github.com/dtrh235/-stm32f407-/releases)** — 按标签下载源码与 Release 说明

当前主线：**v1.5.0**（`main`）

## 仓库

https://github.com/dtrh235/-stm32f407-.git
