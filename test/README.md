# 硬件自检工程（test 目录）

| 工程 | 路径 | 说明 |
|------|------|------|
| **全模块一体自检（推荐）** | `test\nfc_enroll_rs485\MDK-ARM\nfc_enroll_test.uvprojx` | LCD + W25Q + 触摸 + NFC + 指纹 + WiFi + 继电器 + 蜂鸣 + 语音，RS485 115200 |

详细接线、日志判定见 **`test\nfc_enroll_rs485\README.md`**。

**不修改 Host/Client 主工程**；自检结论用于区分硬件接线问题 vs 主程序/UI 流程问题。

## PC 模拟主从 + App（无 RS485 线）

```powershell
cd JoyfulZone/server
npm run test:rs485   # RS485 0x10 帧 CRC
npm run test:sim     # 从机记录注入 + App 远程开锁
```
