# modelkey — STM32F407 智能门锁

STM32F407 双机 Keil 工程：**Host（主控 + 屏 + WiFi/云）**、**Client（从机）**。

| 目录 | 说明 |
|------|------|
| `Host/` | 主控固件：LVGL UI、ESP8266、阿里云 MQTT、NFC、指纹、OTA |
| `Client/` | 从机固件 |
| `Host/test/` | 硬件单项测试（如 NFC + RS485） |

## 工程入口

- 主程序：`Host/Projects/MDK-ARM/stm32f407_lvglport.uvprojx`
- NFC/RS485 测试：`Host/test/nfc_enroll_rs485/MDK-ARM/nfc_enroll_test.uvprojx`

## 版本与更新说明

**每个上传到 GitHub 的版本做了什么，请看：**

- **[CHANGELOG.md](./CHANGELOG.md)** — 中文详细更新记录（推荐）
- **[GitHub Releases](https://github.com/dtrh235/-stm32f407-/releases)** — 按标签下载源码与 Release 说明

当前主线：**v1.3.0**（`main`）

## 仓库

https://github.com/dtrh235/-stm32f407-.git
