# 第 10 课：FreeRTOS 多任务分工

> **阶段**：系统架构 | **建议课时**：3～4 小时

---

## 上节复习

- 开锁事件队列、`app_unlock_event_gui_pump`
- HomeAuth 与 GuiTask 分工
- UART4 开门、云端上报

**预习下节（第 11 课）**：CloudTask 里跑 RS485 主机轮询，下节专讲帧格式。

---

## 学习目标

- 背熟 Host 主要任务表（优先级、栈、职责）
- 理解「为什么 GUI 最高优先级」
- 知道跨任务不能做什么

---

## 本课要读的文件

| 文件 | 关注什么 |
|------|----------|
| `Host/User/main.c` | 任务创建、`xTaskCreate`、优先级宏 |
| `FreeRTOSConfig.h` | 堆大小、钩子函数 |
| `Host/User/app_ccm_ram.c` | CCM 内存用途 |
| `Host/User/main.c` | `app_enroll_flow_active()` |

---

## Host 任务表

| 任务 | 优先级 | 栈(字) | 职责 |
|------|--------|--------|------|
| **GuiTask** | 5（最高） | 1536 | LVGL、`lv_timer_handler`、ui_v3、触摸、开锁弹窗泵 |
| **CloudTask** | 2～4 | 4096 | ESP8266 AT、MQTT、RS485 主站轮询 |
| **FpTask** | 3 | 768 | 指纹录入/硬件相关 |
| **NfcTask** | 3 | 768 | NFC 硬件相关 |
| **HomeAuth** | 3 | 768 | 首页 NFC/指纹轮询开锁 |
| **StorageTask** | 1（最低） | 1536 | W25Q 异步刷盘 |

Client 额外有 **Rs485Slave** 任务跑 `app_rs485_slave_server_poll`。

---

## 通俗理解

- **GuiTask 要快**：5ms 一次 `lv_timer_handler`，保证 ui_v3 流畅。
- **CloudTask 栈最大**：WiFi 扫描 `CWLAP`、AT 解析、MQTT 嵌套深。
- **Storage 最慢优先级**：擦 Flash 几十毫秒，不能饿死 UI。
- **HomeAuth 独立**：生物识别轮询不阻塞 GUI，但也不直接碰 LVGL。

---

## 协作机制

```
其他任务 ──入队/置标志──► GuiTask（唯一碰 LVGL）
CloudTask ◄──RS485/MQTT──► 从机 / 云端
StorageTask ◄──dirty 标志── app_user_ops
app_enroll_flow_active() → 录入时 Cloud/HomeAuth 让步
```

---

## 动手实验

1. 在 `main.c` 搜索所有 `xTaskCreate`，列出任务名。
2. 开 `APP_RTOS_HEARTBEAT_DEBUG=1` 看 CloudTask 心跳（注意 Flash）。
3. 查 `configCHECK_FOR_STACK_OVERFLOW` 是否启用。

---

## 易错点 / 困难点

- 栈溢出：CloudTask 4096 仍可能不够，看溢出钩子。
- 在 CloudTask 里调 LVGL = 经典坑。
- 同一 UART（USART2）只能 CloudTask 统一发 AT。
- `app_enroll_flow_active()`：ui_v3 录入或 screen6/8/9/10 时返回真。

---

## 本课总结

- 按「实时性」分任务，不按「文件模块」简单分
- 跨任务用队列/标志，不直接调 UI
- CloudTask 是 RS485 主机和云的枢纽

---

## 本课提问

1. 为什么 CloudTask 栈要 4096？
2. GuiTask 周期多少 ms（ui_v3 路径）？
3. `app_unlock_event_gui_pump` 放在哪个任务？
4. CCM RAM 在本项目用途？

---

## 参考答案

1. ESP8266 AT、WiFi 列表、MQTT 解析嵌套深，栈小易溢出。
2. 5ms `vTaskDelayUntil`。
3. GuiTask。
4. 把部分缓冲放 CCM（CPU 专用 64KB），减轻 SRAM 压力。

---

**上一课**：[09-开锁流程与事件上报.md](./09-开锁流程与事件上报.md)  
**下一课**：[11-RS485协议与帧格式.md](./11-RS485协议与帧格式.md)
