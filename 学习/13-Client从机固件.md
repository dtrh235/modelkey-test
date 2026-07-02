# 第 13 课：Client 从机固件

> **阶段**：系统架构 | **建议课时**：3 小时

---

## 上节复习

- 用户/指纹镜像、代匹配、`SLAVE_UNLOCK_NOTIFY`
- RS485 全量镜像流程

**预习下节（第 14 课）**：主机独有 ESP8266 WiFi/MQTT，AT 状态机。

---

## 学习目标

- 列出 Client 相对 Host 裁剪了哪些功能
- 理解 IWDG 在从机的意义
- 知道从机 UI 如何简化

---

## 本课要读的文件

| 文件 | 关注什么 |
|------|----------|
| `Client/User/main.c` | 任务表、IWDG、Rs485Slave |
| `Client/User/app_config.h` | `APP_RS485_NODE_ROLE` 必须为 SLAVE |
| `Client/User/app_rs485_proto.c` | 从机侧协议 |
| `Client/User/app_slave_fp_sync.c` | 指纹队列 |
| `Client/User/app_iwdg` 相关 | 看门狗喂狗 |

---

## Host vs Client 对比

| 项目 | Host | Client |
|------|------|--------|
| WiFi/云 | 有 CloudTask | **无** |
| UI | ui_v3 全功能 |  mainly 侧门开锁屏 |
| RS485 角色 | Master poll | **Slave server poll** |
| 用户权威 | 是 | 从 Host 镜像 |
| IWDG | 可选/无 emphasis | **常开** |
| 临时密码 | 本地 + 下发从机 | 接收 RS485 同步 |

---

## Client main 流程要点

1. 同样 `users_storage_load()`（数据来自镜像）
2. `app_temp_password_init`
3. **IWDG 初始化**，主循环/任务中 feed
4. UI：`enter_screen_1()` + `app_slave_ui_fixup` 隐藏多余按钮
5. 任务：**Rs485Slave** 专门 `app_rs485_slave_server_poll`

---

## 从机 FpTask 顺序（再强调）

```
先处理 RS485 指纹模板写入队列
    → 再轮询侧门指纹开锁（或代匹配上传）
```

否则模板未写完就 Search/代匹配会失败。

---

## 动手实验

1. 打开 Client 工程，对比 User 目录文件列表与 Host。
2. 确认 `Client/User/app_config.h` 中 `APP_RS485_NODE_ROLE` 为 `APP_RS485_ROLE_SLAVE`。
3. 双机联调：仅 Client 上电，观察是否发 `MIRROR_SYNC_REQ`。

---

## 易错点 / 困难点

- **两块板都烧 Host** → 双主机，RS485 冲突。
- 从机也烧错成 Host → 侧门仍尝试连 WiFi（若宏错）。
- 从机用户表过期 → 需 Host 在线做一次镜像。
- 看门狗未 feed → 从机周期性复位（查是否卡死在阻塞里）。

---

## 本课总结

- Client = Host 子集 + 从机 RS485 服务 + IWDG
- 侧门 UI 简单，复杂度在同步协议
- 联调必须两块板 + 正确角色宏

---

## 本课提问

1. Client 有没有 CloudTask？
2. 从机为什么需要 IWDG？
3. `app_rs485_slave_server_poll` 在哪个任务调用？
4. 从机用户表更新了谁触发？

---

## 参考答案

1. 没有（无 WiFi/MQTT 任务）。
2. 侧门可能无人看守，死机需自动复位恢复服务。
3. Client `main.c` 的 Rs485Slave 任务（或等价循环）。
4. 主机经 RS485 `USER_ADD/DELETE` 及镜像请求下发。

---

**上一课**：[12-主从同步与指纹镜像.md](./12-主从同步与指纹镜像.md)  
**下一课**：[14-ESP8266与阿里云MQTT.md](./14-ESP8266与阿里云MQTT.md)
