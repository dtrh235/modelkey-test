# 第 6 课：NFC 读卡 MFRC522

> **阶段**：生物识别 | **建议课时**：3～4 小时

---

## 上节复习

- I2C 触摸、坐标镜像、首页双击逻辑

**预习下节（第 7 课）**：刷卡得到 UID，指纹是更复杂的 512 字节模板。

---

## 学习目标

- 掌握 MFRC522 软 SPI 接线与复位时序
- 理解 NFC 开锁：读 UID → 查用户表
- 知道录入与首页轮询为何互斥

---

## 本课要读的文件

| 文件 | 关注什么 |
|------|----------|
| `Host/Drivers/BSP/mfrc522/MFRC522.c` | 底层 SPI 与寄存器 |
| `Host/Drivers/BSP/mfrc522/MFRC522.h` | API |
| `Host/User/app_nfc_hw.c` | 初始化、软复位 |
| `Host/User/app_home_unlock.c` | 首页 NFC 轮询开锁 |
| `Host/User/app_state.h` | `nfc_op_t`、录入状态 |
| `Host/User/main.c` | `app_nfc_hw_init_once` 调用时机 |

---

## 专有名词

| 名词 | 解释 |
|------|------|
| **MFRC522** | NXP 13.56MHz 读卡芯片 |
| **UID** | 卡唯一标识，本项目用 4 字节 |
| **软复位** | 写复位寄存器，上电后必须做才能稳定寻卡 |
| **寻卡/防冲突** | RFID 协议层读卡流程 |

---

## 功能怎么实现

### 硬件

- 软 SPI：PB12=SDA, PB13=SCK, PB14=MISO, PB15=MOSI
- RST：PD1

### 初始化

`app_nfc_hw_init_once()` 在 `main.c` 里调用。  
注释强调：**不做软复位，首页可能刷不了卡**。

### 开锁流程

```
HomeAuth 任务 ~200ms 轮询
    → 读卡得 UID[4]
    → user_auth_find_index_by_nfc_uid()
    → 用户 active 且 has_nfc
    → 触发开锁事件
```

### 录入流程

在 NFC 管理/screen8 录入页绑定 UID 到指定账号 `user_auth_bind_nfc_by_acc()`。  
录入激活时 `app_enroll_flow_active()` 为真，首页 NFC 轮询应暂停，避免冲突。

---

## 动手实验

1. 自检工程 `TEST_ENABLE_NFC_HW=1`，刷卡看 `[NFC] CARD UID=...`。
2. 主程序首页刷卡，观察是否开锁（需已绑定用户）。
3. 对比有/无 `app_nfc_hw_init_once` 时读卡差异。

---

## 易错点 / 困难点

- 忘记软复位 → 有版本号 `0x91/0x92` 但寻卡失败。
- NFC UID 可被克隆，本项目是**本地白名单**，非金融级安全。
- NFC 与指纹在 HomeAuth 里**交叉轮询**（各约 200ms），避免同时抢 SPI/CPU。
- 录入页与首页共用同一 MFRC522，必须互斥。

---

## 本课总结

- MFRC522 软 SPI，**必须正确复位时序**
- UID 存在 `user_cred_t.nfc_uid[4]`
- 首页 NFC 在 HomeAuth 任务轮询，与录入互斥

---

## 本课提问

1. MFRC522 复位脚是哪个？
2. 开锁时如何用 NFC UID 找到用户？
3. 为什么录入时要暂停首页 NFC 轮询？
4. 日志 `[NFC] ver=0x91` 说明什么？

---

## 参考答案

1. PD1。
2. 调用 `user_auth_find_index_by_nfc_uid(users, count, uid)` 得索引，再检查 `active` 和 `has_nfc`。
3. 避免录入过程中首页误读卡或 SPI/状态机冲突。
4. 版本寄存器读成功，SPI 通信基本正常。

---

**上一课**：[05-电容触摸FT6336.md](./05-电容触摸FT6336.md)  
**下一课**：[07-指纹模块AS608.md](./07-指纹模块AS608.md)
