# 硬件自检工程（test 目录）

| 工程 | 路径 | 说明 |
|------|------|------|
| **一体自检（推荐）** | `test\test\MDK-ARM\nfc_enroll_test.uvprojx` | W25Q + NFC + **触屏** + **开锁 1/1111 账号诊断**，RS485 115200 |

详细接线、日志判定、跳过已测硬件的宏开关见 **`test\test\README.md`**。

**不修改 Host/Client 主工程**；诊断结论用于区分触屏硬件问题 vs Flash 账号数据问题 vs 主程序 UI 流程问题。
