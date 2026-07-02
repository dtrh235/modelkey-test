# 第 15 课：云端桥接与 Topic

> **阶段**：联网与 UI | **建议课时**：3～4 小时

---

## 上节复习

- ESP8266 AT、CloudTask 独占 USART2
- MQTT 连接与 WiFi 页协调

**预习下节（第 16 课）**：界面用 ui_v3 手写，导航栈与 widgets。

---

## 学习目标

- 分清「自定义 Topic 桥接」与「物模型上报」两条路
- 知道开锁记录、远程开锁各自走哪条 Topic
- 理解上报失败队列机制

---

## 本课要读的文件

| 文件 | 关注什么 |
|------|----------|
| `README.md` | 云端数据通路表 |
| `Host/User/app_config.h` | `APP_BRIDGE_TOPIC_*` |
| `Host/User/cloud_ota_service.c` | 上报、队列、物模型 |
| `Host/User/app_cloud_command.c` | `terminal/get` 下行处理 |
| `JoyfulZone/server/TOPIC_SETUP.md` | 阿里云产品流转配置 |
| `JoyfulZone/server/README.md` | 后端启动 |

---

## 两条云端通路

| 用途 | 通路 | 说明 |
|------|------|------|
| App 开锁记录 / 用户同步 / 临时密码 | 门锁 `terminal/push` → 流转 → 后端 `server/push` | **JoyfulZone 主路径** |
| 手机远程开锁 / bind / 临时密码下发 | 后端 `server/get` → 流转 → 门锁 `terminal/get` | 下行控制 |
| 阿里云控制台历史 | `/sys/.../thing/event/property/post` | 物模型，与 App 桥接**独立** |

宏定义：

```c
#define APP_BRIDGE_TOPIC_TERMINAL_PUSH  "/user/terminal/push"
#define APP_BRIDGE_TOPIC_TERMINAL_GET   "/user/terminal/get"
```

---

## 功能怎么实现

### 上行（开锁记录）

```
开锁成功（主机或从机汇总）
    → cloud_ota_service 组 payload
    → MQTT publish terminal/push
    → Node 后端写入 unlock_records.json 等
    → App 拉取/推送展示
```

字段与物模型对齐：`unlock_method`、`unlock_time`、`unlock_account` 等。

### 下行（远程开锁）

```
App 请求 → 后端 server/get
    → 云流转 → 门锁 terminal/get
    → app_cloud_command 解析
    → 执行远程开锁 / 临时密码 / 配对相关
```

### 可靠性

- RAM 队列缓存
- `app_unlock_flash_queue.c` Flash 备份
- seq 去重（防重复上报）

---

## 动手实验

1. 读 `JoyfulZone/server/TOPIC_SETUP.md`，理解产品流转规则。
2. 本地 `npm start` 后端，观察 `src/data/unlock_records.json`。
3. 运行 `JoyfulZone/server/scripts/host-slave-app-sim.js`（无硬件模拟）。

---

## 易错点 / 困难点

- 阿里云控制台**未配置流转** → Topic 通了但后端收不到。
- `.env` DeviceSecret 勿提交 Git。
- 物模型字段名与固件 JSON 不一致 → 控制台看不到数据。
- Client 无 MQTT，从机记录必须经 Host 转发。

---

## 本课总结

- 自定义 Topic 连 Node 后端；物模型连阿里云控制台
- 可靠性靠队列 + Flash 备份
- 四端一致：Host / Client / App / 后端

---

## 本课提问

1. App 远程开锁走哪条 Topic 方向？
2. 从机 NFC 开锁 `method_id` 和 `device_id` 各是多少？
3. 后端模拟脚本在哪？
4. 开锁记录上报失败存哪？

---

## 参考答案

1. 后端 `server/get` → 云流转 → 门锁 `terminal/get`。
2. `method_id=2`（NFC），`device_id=2`（从机）。
3. `JoyfulZone/server/scripts/`（如 `host-slave-app-sim.js`）。
4. `app_unlock_flash_queue.c` 持久化队列。

---

**上一课**：[14-ESP8266与阿里云MQTT.md](./14-ESP8266与阿里云MQTT.md)  
**下一课**：[16-UI-v3架构.md](./16-UI-v3架构.md)
