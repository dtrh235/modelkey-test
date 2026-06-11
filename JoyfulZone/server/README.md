# Joyful Zone 后端（本机调试）

App 与门锁之间的中间层。后端以 **服务器 IoT 设备** 身份连 MQTT，经 **云产品流转** 与门锁 `1111` 交换消息（无需 AccessKey）。

## 启动

```powershell
cd /d "D:\keil project\modelkey\JoyfulZone\server"
copy .env.example .env
# 编辑 .env 填写 ALIYUN_SERVER_DEVICE_NAME / SECRET
npm install
npm start
```

测试：

```powershell
curl http://127.0.0.1:3000/health
curl http://127.0.0.1:3000/devices/status
```

`health` 中 `mqttConnected: true` 表示后端 MQTT 已连上阿里云。

## 配置（.env）

| 变量 | 说明 |
|------|------|
| `ALIYUN_SERVER_DEVICE_NAME` | 控制台「服务器」设备 DeviceName |
| `ALIYUN_SERVER_DEVICE_SECRET` | 该设备 DeviceSecret（**勿提交 Git**） |
| `ALIYUN_LOCK_DEVICE_NAME` | 门锁 `1111` |
| `ALIYUN_IOT_PRODUCT_KEY` | 产品 `k1vtgud0XCS` |

## 阿里云：添加自定义 Topic（必做）

路径：**物联网平台 → 设备管理 → 产品 → hhwifi → Topic 列表 → 定义 Topic 类**

对每个后缀点 **定义 Topic 类**，按下面填：

| Topic 类（后缀） | 操作权限 |
|------------------|----------|
| `terminal/push` | **发布** |
| `terminal/get` | **订阅** |
| `server/push` | **订阅**（后端收门锁回复） |
| `server/get` | **发布**（后端发指令） |

说明：

- 「Topic 类」只写 **`terminal/push`** 这种后缀，不要写 `user/`、ProductKey、DeviceName。
- 同一产品下所有设备（`1111` 和服务器设备）共用这套 Topic 类。
- 保存后等约 1 分钟生效。

### 云产品流转

详见 [TOPIC_SETUP.md](./TOPIC_SETUP.md)。

解析器 **必须点「启动」**，否则消息不会转发。

## App 配对 API

```http
POST /devices/bind
Content-Type: application/json

{ "pairCode": "482913", "appAccount": "1001" }
```

- 后端 publish `server/get` → 流转 → 门锁 `terminal/get`
- 门锁校验后 publish `terminal/push` → 流转 → 后端 `server/push`
- **最多等 5 秒** `bind_ack` 再返回成功/失败

成功：`ok: true, bound: true`  
配对码错误：`BIND_REJECTED`  
超时：`BIND_ACK_TIMEOUT`（门锁离线或流转未启动）

## 目录

```
src/
  index.js
  config.js
  routes/
  services/aliyunMqtt.js
  data/bindings.json
```
