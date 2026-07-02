# 云产品流转配置清单

门锁 `1111` ↔ 后端设备 `uZdKY8WoRcIaVtaIrwWh`，经两条自定义 Topic 桥接。

## Topic 全名对照

| 角色 | 发布 (publish) | 订阅 (subscribe) |
|------|----------------|------------------|
| 门锁 1111 | `/k1vtgud0XCS/1111/user/terminal/push` | `/k1vtgud0XCS/1111/user/terminal/get` |
| 后端服务器 | `/k1vtgud0XCS/uZdKY8WoRcIaVtaIrwWh/user/server/get` | `/k1vtgud0XCS/uZdKY8WoRcIaVtaIrwWh/user/server/push` |

数据流：

```
App → 后端 publish server/get
    → 规则「服务器→终端」
    → 门锁收到 terminal/get

门锁 publish terminal/push
    → 规则「终端→服务器」
    → 后端收到 server/push
```

---

## 第一步：产品 Topic 类（若未添加）

产品 **hhwifi** → **Topic 列表** → **定义 Topic 类**

分别新建 4 个 Topic 类（只填后缀列里的路径）：

1. `/user/terminal/push` — 操作：**发布**
2. `/user/terminal/get` — 操作：**订阅**
3. `server/push` — 操作：**订阅**（后端 MQTT 订阅收 bind_ack）
4. `server/get` — 操作：**发布**（后端 MQTT 发布 bind 指令）

---

## 第二步：数据源

**规则引擎 → 云产品流转 → 数据源**

### 数据源「门锁终端」

添加 Topic（设备上报触发）：

```
/k1vtgud0XCS/1111/user/terminal/push
```

### 数据源「门锁服务器」

添加 Topic：

```
/k1vtgud0XCS/uZdKY8WoRcIaVtaIrwWh/user/server/get
```

---

## 第三步：数据目的

**云产品流转 → 数据目的**

### 目的「服务器」（终端→服务器用）

- 类型：**发布到 IoT Topic**
- Topic 全名：

```
/k1vtgud0XCS/uZdKY8WoRcIaVtaIrwWh/user/server/push
```

### 目的「终端」（服务器→终端用）

- 类型：**发布到 IoT Topic**
- Topic 全名：

```
/k1vtgud0XCS/1111/user/terminal/get
```

---

## 第四步：解析器脚本

### 解析器「终端--服务器」

- 关联数据源：**门锁终端**
- 关联数据目的：**服务器**
- 脚本（透传）：

```javascript
module.exports = function (payload, topic) {
    return payload;
};
```

若页面提供 `transformPayload`：

```javascript
function transformPayload(payload) {
    return payload;
}
```

### 解析器「服务器--终端」

- 关联数据源：**门锁服务器**
- 关联数据目的：**终端**
- 脚本：

```javascript
module.exports = function (payload, topic) {
    return payload;
};
```

---

## 第五步：启动解析器

在 **解析器** 列表对两条规则点 **启动**。状态应为「运行中」，不是「未启动」。

---

## 第六步：激活服务器设备

后端第一次 `npm start` 会用 `uZdKY8...` 的 DeviceSecret 连 MQTT。  
控制台该设备状态应从 **未激活** 变为 **已激活**。

`GET http://127.0.0.1:3000/health` 应显示 `"mqttConnected": true`。

---

## 消息格式（cmd 信封）

**下行 bind（后端 → 门锁）：**

```json
{"cmd":"bind","pairCode":"482913","appAccount":"1001","id":"1738123456789"}
```

**上行 bind_ack（门锁 → 后端）：**

```json
{"cmd":"bind_ack","ok":true,"appAccount":"1001","id":"1738123456789"}
```

`id` 用于后端匹配 5 秒内的响应。

---

## 第七步：开锁记录同步（物模型 → 后端）

门锁每次开锁会上报物模型属性（Topic：`/sys/{ProductKey}/1111/thing/event/property/post`）。

### 物模型字段（与固件一致，无需再改枚举）

| 标识符 | 类型 | 取值 |
|--------|------|------|
| `unlock_account` | text | 开锁账号，如 `1001` |
| `unlock_time` | text | `YYYY.MM.DD HH:MM`（墙钟同步后） |
| `unlock_method` | enum | `1`password `2`nfc `3`fingerprint `4`phone `5`temporary-password |
| `unlock_device` | int | `1`门锁主屏 `2`侧门从机 `3`手机 |
| `unlock_account` | text | 普通用户账号；远程固定 **`0`**；临时密码固定 **`temporary account`** |

**主机与从机 `unlock_method` 一致**（与阿里云控制台枚举一一对应），区别仅在于：

| method | 含义 | 主机 | 从机（RS485→主机上报） |
|--------|------|------|------------------------|
| 1 | password | ✅ device=1 | ✅ device=**2** |
| 2 | nfc | ✅ device=1 | ✅ device=**2** |
| 3 | fingerprint | ✅ device=1 | ✅ device=**2** |
| 4 | phone | ✅ device=3，account=0 | ❌ 从机无远程开锁 |
| 5 | temporary-password | ✅ device=1 | ✅ device=**2**，account=temporary account |

固件 `cloud_ota_service.c` 已按上表上报；**手机远程开锁**仅主机：`method=remote` → `unlock_method=4`。

### 后端收记录（推荐：门锁经 terminal/push 桥接）

**与 bind_ack 同一条已验证通路**（固件 `cloud_ota_service.c` 每次开锁会 publish）：

```
门锁 terminal/push  {"cmd":"unlock_record", ...}
    → 解析器「终端--服务器」
    → 后端 server/push
```

后端识别 `cmd=unlock_record`，无需再配物模型流转即可在 App 看到记录。

**备选 B：物模型 Topic 流转**

- 数据源：`/sys/k1vtgud0XCS/1111/thing/event/property/post`
- 数据目的：`/k1vtgud0XCS/uZdKY8WoRcIaVtaIrwWh/user/server/push`
- 解析器脚本：

```javascript
var data = payload('json');
writeIotTopic(1003, "/k1vtgud0XCS/uZdKY8WoRcIaVtaIrwWh/user/server/push", data);
```

**备选 C：后端直接订阅物模型**（需在 Topic 类给服务器设备加订阅权限，日志出现 `subscribed ... property/post` 才生效）

### App 拉取（3 个月历史）

```
GET /records?deviceName=1111&months=3
GET /records?deviceName=1111&months=3&refresh=1   # 强制从阿里云重新拉物模型历史
```

后端在 `.env` 配置 `ALIYUN_ACCESS_KEY_ID` / `ALIYUN_ACCESS_KEY_SECRET` 后，会调用
`QueryDevicePropertyData` 拉取门锁物模型 4 个属性历史，与实时 `terminal/push` 记录合并去重。

- **App 实时**：`terminal/push` → 后端（优先、更全）
- **App 历史**：阿里云物模型 API（门锁上传后本地已删的数据也在这里）
- **固件**：物模型失败自动重试队列，不影响 App 通道

远程开锁成功后，后端也会在 `POST /devices/unlock` 成功时补写一条 `unlock_account=0 / unlock_method=4 / unlock_device=3` 记录（与门锁上报去重）。

### 安全告警（密码连续错误 5 次）

门锁经 `terminal/push` 上报用户变更（与 App 双向同步）：

```json
{"cmd":"user_changed","action":"add|delete|password|role|nfc_bind|nfc_delete|fp_bind|fp_delete","account":"1003","isAdmin":0,"hasPwd":1,"hasNfc":0,"hasFp":0,"id":"..."}
```

门锁经 `terminal/push` 上报：

```json
{"cmd":"unlock_alert","device":1,"failCount":5,"lastAccount":"1002","id":"..."}
```

`device`：`1`主屏 `2`侧门。后端写入通知中心，App `GET /notifications` 拉取。

---

## 常见问题

| 现象 | 检查 |
|------|------|
| `mqttConnected: false` | `.env` 凭证、服务器设备是否激活 |
| `BIND_ACK_TIMEOUT` | 门锁 MQTT 在线、解析器已启动、Topic 类权限 |
| 门锁串口无 `[CLOUD][BIND]` | 固件是否已烧录新 Topic；是否订阅 `terminal/get` |
| 流转不生效 | 数据源 Topic 是否写全路径；目的 Topic 是否写对设备名 |
