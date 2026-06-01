# NFC 录入 + RS485 调试测试工程

独立 Keil 工程，用于在**不跑 LVGL/云端**的情况下验证：

- MFRC522 读卡 / 录入（`NFC_Enroll_Detect` + `Flash_AddID`）
- RS485 调试口（USART6 `PC6/PC7/PC8`，115200）向串口助手打印结果

## 目录

```
Host/test/nfc_enroll_rs485/
  User/           测试 main、app_config、RS485 日志
  MDK-ARM/        Keil 工程 nfc_enroll_test.uvprojx
  Output/         编译输出（生成后）
```

## 硬件接线

| 功能 | 引脚 | 说明 |
|------|------|------|
| RS485 TX | PC6 | 接模块 DI |
| RS485 RX | PC7 | 接模块 RO |
| RS485 DE | PC8 | 高=发送，低=接收 |
| MFRC522 | PB12-15, PD1 RST | 与 Host 工程相同 |

USB 转 RS485 接总线，串口助手：**115200 8N1**。

## 打开工程

1. Keil μVision 打开：`Host\test\nfc_enroll_rs485\MDK-ARM\nfc_enroll_test.uvprojx`
2. Target 选 `stm32f407vet6`，编译、下载。
3. 上电后应看到类似输出：

```
[NFC_TEST] NFC enroll hardware test
[NFC_TEST] waiting card...
[NFC_TEST] CARD UID=04 A1 B2 C3
[NFC_TEST] ENROLL OK UID=04 A1 B2 C3
[NFC_TEST] saved total=1
```

## 判定

| 日志 | 含义 |
|------|------|
| `MFRC522 ver=0x91/0x92 ready=1` | 读卡芯片正常 |
| `CARD UID=...` | 刷卡成功 |
| `ENROLL OK` | 已写入 RAM/Flash 卡库 |
| `ENROLL SKIP card already exists` | 重复卡 |
| `ENROLL FAIL storage full` | 卡库已满（最多 10 张） |
| 无任何输出 | 检查 RS485 接线、DE、A/B 反接、波特率 |

## 与正式 Host 工程关系

- 复用 `Host/Drivers/BSP/mfrc522`、`Host/User/app_nfc_hw.c`、`app_rs485_link.c`
- `User/app_config.h` 仅打开 RS485，关闭 WiFi/LVGL/FreeRTOS
- **勿与正式工程同时烧录到同一块板子做 A/B 对比时混淆 Output 目录**

## 常见问题

1. **有 alive 无刷卡**：天线/刷卡距离；看 `nfc_ready` 是否为 1。
2. **ver=0x00/0xFF**：SPI 接线或 RC522 供电。
3. **乱码**：波特率非 115200，或 RS485 未共地。
