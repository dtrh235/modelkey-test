# PCB4_5 全模块硬件自检（RS485 日志）

独立 Keil 工程，**不修改 Host 主固件**。上电后**一次性**跑完所有模块，末尾打印 **SUMMARY** 表；之后进入交互轮询（指纹 STA、触摸、NFC 刷卡）。

## 打开工程

`test\nfc_enroll_rs485\MDK-ARM\nfc_enroll_test.uvprojx` → **Rebuild** → Download

## 日志口

| 信号 | MCU 引脚 | 说明 |
|------|----------|------|
| DI_485 (TX) | PC6 | USART6_TX |
| RO_485 (RX) | PC7 | USART6_RX |
| EN_485 (DE) | PC8 | 发送前拉高 |

USB-RS485 接 **H11** A/B，串口助手 **115200 8N1**。

## 模块与接线（与 Host / 网表 PCB4_5 一致）

| 模块 | 引脚 | 自检项 |
|------|------|--------|
| LCD ILI9341 | SPI1 PA5/6/7，PE4=RST PE6=DC PE7=CS | 彩条 + 读 ID |
| W25Q16 | PE0=CS PB3=SCK PB4=MISO PB5=MOSI | JEDEC 0xEF4015 + 擦写读 |
| Touch FT6336 | PB6/7 I2C，PB1=INT PE8=RST | I2C 扫描 + chip id |
| NFC MFRC522 | PB12-15 SPI，PD1=RST | VersionReg 0x91/0x92 |
| FP AS608 | USART3 PB10/11，WAK PE10 | 握手 + 模块信息 |
| WiFi WF24 | USART2 PA2/3，KEY PE12 | KEY 复位 + `AT` |
| Relay H9 | PE9 → RELAY1 | **不测** |
| Buzzer | PC9 | **不测** |
| Voice U34 | UART4 PC10/11 | **不测** |

## 上电流程

1. LCD → W25Q → 触摸 → NFC → 指纹 → WiFi（约 5s）  
2. 打印 `HW ALL-IN-ONE TEST SUMMARY`  
3. 持续：`[FP] STA=…`、`[TP] DOWN/UP`、`[NFC] CARD UID=…`

## SUMMARY 判定

| 行 | PASS 含义 | FAIL 常见原因 |
|----|-----------|---------------|
| RS485 log | 日志口正常 | H11/DE/波特率 |
| LCD | id≠0，屏幕有彩条 | PE4/6/7、SPI 线 |
| W25Q16 | JEDEC + 读写 | PE0/PB3-5 虚焊 |
| Touch | I2C ACK 0x70 | PB6/7、FPC、上拉 |
| NFC | ver 0x91/0x92 | CN10 电源/SPI |
| FP AS608 | 握手成功 | CN7 RX PB11、波特率 |
| WiFi WF24 | 收到 `OK` | H5 供电、PA2/3 |

继电器 / 蜂鸣 / 语音 / RS485 日志口**不在自检列表**（日志口仅用于输出）。

## 宏开关（`User/app_config.h`）

- `TEST_ENABLE_LCD_HW=0` — 跳过 LCD（仅串口测其它模块）
- `TEST_USB_UART_HEARTBEAT_MS` — 心跳间隔，0=关闭

## 说明

- 与 Host 共用 BSP 源文件（`lcd.c`、`touch.c`、`MFRC522.c` 等），引脚定义与主工程一致。  
- WiFi 自检约 5s（KEY 复位等待），属正常现象。  
- NFC `reset_ret=0x26`（MI_OK）**不能**代替 VersionReg 判断芯片好坏。
