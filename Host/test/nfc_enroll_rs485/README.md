# W25Q16 外部 Flash + MFRC522 NFC 焊板自检（RS485）

独立 Keil 工程，**不依赖 LVGL/WiFi**。一次烧录同时测：

- **W25Q16** 外部 Flash（JEDEC + 擦写读）
- **MFRC522** NFC 刷卡
- **RS485** 打印（USART6 PC6/PC7/PC8，115200）

## 打开工程

`Host\test\nfc_enroll_rs485\MDK-ARM\nfc_enroll_test.uvprojx` → Rebuild → Download

## 接线（与 Host 主工程一致）

| 功能 | 引脚 |
|------|------|
| RS485 | PC6→DI, PC7←RO, PC8=DE |
| W25Q16 | PA15=CS, PB3=SCK, PB4=MISO, PB5=MOSI |
| MFRC522 | PB12–PB15, PD1=RST |

USB-RS485 接 A/B，串口助手 **115200 8N1**。

## 上电日志示例

```
[HW_TEST] W25Q16 ext Flash + MFRC522 NFC
[HW_TEST] RS485OK
[W25Q] JEDEC=0xEF4015 (expect 0xEF4015)
[W25Q] JEDEC OK
[W25Q] PASS erase/write/read
[NFC] MFRC522 ver=0x91 (OK=0x91/0x92) ...
[NFC] waiting card...
```

## 判定

| 日志 | 含义 |
|------|------|
| `[W25Q] JEDEC OK` + `PASS` | 外部 Flash 正常 |
| `[W25Q] FAIL` / JEDEC 非 0xEF4015 | Flash 未焊好或 SPI 线错 |
| `[NFC] ver=0x91/0x92` | NFC 芯片通信正常 |
| `[NFC] ver=0x00` + FAIL 行 | NFC 硬件异常（你之前的情况） |
| `[NFC] CARD UID=...` | 刷卡成功 |

## 说明

- `reset_ret=38` 即 `0x26`（MI_OK），**不能**代替 `ver` 判断 NFC 好坏。
- `flash card_count` 是 NFC 录入用的**片内 Flash 计数**，不是 W25Q16。
