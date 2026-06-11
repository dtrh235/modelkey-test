# 焊板自检：W25Q + NFC + 触屏 + 开锁账号诊断

独立 Keil 工程，**不修改 Host/Client 主工程**。一次烧录通过 RS485 串口完成：

| 项目 | 说明 |
|------|------|
| W25Q16 | JEDEC + 擦写读 |
| 开锁账号 | 读 Flash 用户表，模拟 `1`/`1111` 校验 |
| FT6336 触屏 | I2C 通信 + 触摸坐标 |
| MFRC522 NFC | 刷卡 |

## 打开工程

`test\test\MDK-ARM\nfc_enroll_test.uvprojx` → Rebuild → Download

## 接线

| 功能 | 引脚 |
|------|------|
| RS485 | PC6→DI, PC7←RO, PC8=DE |
| W25Q16 | **PA15=CS**, PB3=SCK, PB4=MISO, PB5=MOSI |
| MFRC522 | PB12=SDA, PB13=SCK, PB14=MISO, PB15=MOSI, PD1=RST |
| FT6336 | PB6=SCL, PB7=SDA, PB1=INT, PE8=RST |

USB-RS485 **115200 8N1**（不是 PA9 调试口）。

## 跳过已确认正常的硬件

在 `test\test\User\app_config.h` 把对应宏改为 `0`：

```c
#define TEST_ENABLE_W25Q_HW    0   /* W25Q 已 OK */
#define TEST_ENABLE_NFC_HW     0   /* NFC 已 OK */
#define TEST_ENABLE_TOUCH_HW   0   /* 触屏已 OK */
```

`TEST_ENABLE_UNLOCK_SW` 建议保持 `1`（纯软件诊断，不占 GPIO）。

## 日志判定

### 触屏 `[TP]`

| 日志 | 含义 |
|------|------|
| `PASS hardware: FT6336 id=0x51` | I2C + 芯片正常 |
| `FAIL hardware: I2C no ACK` | 触屏硬件/接线问题 |
| `[TP] DOWN x=.. y=..` | 触摸有坐标，硬件可用 |
| 有 DOWN 但主程序仍无效 | **软件/UI**：Host 首页须**双击**「开锁」钮；screen1 点 OK 提交 |

### 开锁 `[AUTH]`

| 日志 | 含义 |
|------|------|
| `simulate unlock... => PASS` | Flash 数据正确，1/1111 应能开锁 |
| `=> 主程序进不了页：优先查触屏/双击` | 数据 OK，问题在触屏或 UI 流程 |
| `simulate unlock... => FAIL` | Flash 中密码/账号与 1111 不符 |
| `cause: 默认管理员密码已改为` | 出厂密码已被改掉 |
| `store INVALID` | Flash 无有效用户表（主程序会 RAM 补 1/1111） |

### W25Q / NFC

与原先一致：`[W25Q] PASS`、`[NFC] ver=0x91/0x92`、`[NFC] CARD UID=...`。

## 主程序 1/1111 进不了页 — 常见原因（本测试帮助区分）

1. **触屏硬件坏**：`[TP] FAIL` 或始终没有 `DOWN`
2. **触屏正常但操作方式**：Host `app_touch_ui` 首页开锁钮需点**两次**（第一次选中，第二次进入）
3. **Flash 密码已改**：`[AUTH] unlock => FAIL` 且打印实际 `pwd='...'`
4. **账号迁移后 users[] 无 1**：`admin_del=1` 且 `account '1' NOT in active user list`
