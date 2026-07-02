# 第 14 课：ESP8266 与阿里云 MQTT

> **阶段**：联网与 UI | **建议课时**：4～6 小时（可分两次读）

---

## 上节复习

- Client 无云、开锁经 Host 上报
- CloudTask 栈最大、独占 USART2

**预习下节（第 15 课）**：MQTT 连上后走 `terminal/push` / `terminal/get` 与后端桥接。

---

## 学习目标

- 知道 ESP8266 接哪个串口、哪些引脚
- 理解 AT 状态机大致阶段（不必一次读完 4000 行）
- 明白 WiFi 页与 MQTT 为何不能同时乱发 AT

---

## 本课要读的文件

| 文件 | 关注什么 |
|------|----------|
| `Host/User/cloud_aliyun_at.c` | 状态枚举、`cloud_aliyun_at_poll` 入口 |
| `Host/User/app_config.h` | 三元组、SNTP、波特率宏 |
| `Host/User/app_wifi_scan.c` | WiFi 扫描 |
| `Host/User/ui_v3/app_ui_v3_screens.c` | WiFi 屏（或 `app_screen_wifi_flow.c`） |

---

## 专有名词

| 名词 | 解释 |
|------|------|
| **ESP8266** | WiFi 模组，串口 AT 控制 |
| **AT 指令** | 如 `AT`、`AT+CWJAP`、`AT+MQTTCONN` |
| **三元组** | ProductKey、DeviceName、DeviceSecret |
| **CWJAP** | 连接 WiFi 热点 |
| **CIPSEND** | 发送 TCP 数据，需先算长度 |
| **SNTP** | 网络对时，`APP_ALIYUN_SNTP_ENABLE` |

---

## 硬件连接

- **USART2**：PA2=TX, PA3=RX
- **RST**：PA8
- 波特率：`APP_ALIYUN_UART_BAUD` = 115200
- `APP_ALIYUN_BAUD_PROBE=0`：不自动切换波特率，避免模组更乱

---

## 功能怎么实现（简化状态机）

```
CloudTask 循环:
  模组复位/就绪
    → WiFi 连接（CWJAP 或记忆热点）
    → MQTT 配置（三元组 + HMAC 密码）
    → MQTT 连接、订阅 terminal/get
    → 循环：publish 开锁记录 / 处理下行指令
```

### WiFi 页独占

进入 WiFi 配置屏时，CloudTask 侧重扫描/连接，避免与其他 AT 交错。  
`APP_WIFI_UI_SCAN_ENABLE` 控制是否真扫描列表。

### SNTP 与 MQTT 时序

`APP_ALIYUN_SNTP_BEFORE_MQTT=0`（默认）：先连 MQTT 更快。  
`=1` 先校时再 MQTT，多等 20s+。  
注释：**SNTP 与 MQTT 不能同时发 AT**。

---

## 动手实验

1. 读 `cloud_aliyun_at.c` 文件头注释和 `enum` 状态定义（前 200 行）。
2. 在 `app_config.h` 找到 `APP_ALIYUN_MQTT_PASSWORD`（HMAC 说明）。
3. 若有模组，开 `APP_WIFI_CWJAP_TRACE=1` 看连接步骤（注意 Flash）。

---

## 易错点 / 困难点

- **同一 UART 不能两个任务同时 AT**。
- 三元组/密码错误 → MQTT 永远连不上。
- `printf` 调试宏会增大 Flash，默认全关。
- WiFi 密码含特殊字符 → CWJAP 转义问题。

---

## 本课总结

- ESP8266 = USART2 + 巨型状态机 `cloud_aliyun_at.c`
- WiFi 页与云端抢模组，靠 CloudTask 内协调
- 三元组在 `app_config.h`，生产环境勿泄露

---

## 本课提问

1. ESP8266 接哪个 USART？
2. 为什么 SNTP 默认不在 MQTT 前阻塞？
3. `APP_ALIYUN_UART_BAUD` 多少？
4. WiFi 扫描为什么需要大栈？

---

## 参考答案

1. USART2，PA2/PA3，RST PA8。
2. 先连 MQTT 更快上线；SNTP 阻塞会多等 20s+。
3. 115200。
4. `CWLAP` 解析 AP 列表回调链深，在 CloudTask 4096 栈里跑。

---

**上一课**：[13-Client从机固件.md](./13-Client从机固件.md)  
**下一课**：[15-云端桥接与Topic.md](./15-云端桥接与Topic.md)
