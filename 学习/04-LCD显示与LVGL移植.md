# 第 4 课：LCD 显示与 LVGL 移植

> **阶段**：存储与显示 | **建议课时**：3～4 小时

---

## 上节复习

- W25Q 用户表布局、`USR1` 魔数
- 启动时尽早 `users_storage_load()` 的原因

**预习下节（第 5 课）**：能显示了还要能点，下节 FT6336 触摸 + `lv_port_indev`。

---

## 学习目标

- 知道 ILI9341 在本项目里走哪组 SPI
- 理解 `lv_port_disp.c` 如何把 LVGL 画面送到 LCD
- 能排查「白屏」类问题

---

## 本课要读的文件

| 文件 | 关注什么 |
|------|----------|
| `Host/Drivers/BSP/LCD/lcd.h` | 分辨率、引脚 |
| `Host/Drivers/BSP/LCD/lcd.c` | ILI9341 初始化与画点 |
| `Host/Middlewares/LVGL/.../lv_port_disp.c` | flush 回调 |
| `Host/User/main.c` | `lv_init` → `lv_port_disp_init` → `lv_refr_now` |
| `Host/Middlewares/LVGL/.../lv_conf.h` | 色深、内存配置 |

---

## 专有名词

| 名词 | 解释 |
|------|------|
| **ILI9341** | 2.8 寸 TFT 屏常用驱动芯片 |
| **硬件 SPI1** | LCD 用 MCU 硬件 SPI，速度快 |
| **DC 引脚** | Data/Command，区分命令字节与像素数据 |
| **LVGL flush** | LVGL 画好一块区域后，回调驱动写入 LCD |
| **双缓冲 / 部分刷新** | 只刷新变化区域，减轻 SPI 负担 |

---

## 功能怎么实现

### 启动显示链路

```
lv_init()
  → lv_port_disp_init()     注册 disp_drv，flush_cb 内部调 lcd
  → app_ui_v3_init()        或 Guider setup_ui() 建界面
  → lv_timer_handler()
  → lv_refr_now()           首帧强制刷新，防白屏
```

### `lv_port_disp.c` 职责

- 分配 draw buffer（大小见 `lv_conf.h`）
- 实现 `flush_cb`：把 LVGL 颜色数据通过 `lcd` 驱动写到 ILI9341
- 刷新完成调用 `lv_disp_flush_ready()`

### 与 W25Q 的关系

- LCD：**硬件 SPI1**
- W25Q：**软 SPI**（另一组 GPIO）
- 不能混用引脚；初始化顺序上 Flash 用户表应先于 LVGL 占满总线时间

---

## 动手实验

1. 在 `main.c` 找到 `lv_port_disp_init` 和 `lv_refr_now` 的调用位置。
2. 设 `APP_BOOT_STAGE_LOG=1`，观察启动停在哪一步（白屏时有用）。
3. 浏览 `lv_conf.h` 中 `LV_COLOR_DEPTH`、`LV_MEM_SIZE`。

---

## 易错点 / 困难点

- **白屏**：未 `lv_refr_now`、背光未开、LCD 初始化失败、GuiTask 未跑 `lv_timer_handler`。
- LCD SPI1 与 W25Q 软 SPI **引脚完全不同**，不要对照错原理图。
- `lv_conf.h` 裁剪不当会导致 RAM 不足或字体缺失。

---

## 本课总结

- ILI9341 走**硬件 SPI1**
- LVGL 通过 `lv_port_disp` 桥接到 `lcd.c`
- 首帧必须 `lv_refr_now` 防白屏
- 显示与 Flash 不同总线，但启动顺序仍要协调

---

## 本课提问

1. 本项目 LCD 用硬件 SPI 还是软 SPI？
2. `lv_port_disp.c` 的核心职责是什么？
3. main 里为什么要 `lv_refr_now`？
4. 白屏时你会查哪三个地方？

---

## 参考答案

1. 硬件 SPI1（引脚见 `lcd.h`，如 PA5/6/7 等）。
2. 注册 LVGL 显示驱动，实现 flush 把刷新区域写到 ILI9341。
3. 初始化后强制刷一帧，避免屏上电后长时间空白。
4. 背光/供电；`lv_port_disp` 是否注册成功；是否持续调用 `lv_timer_handler`/`lv_refr_now`。

---

**上一课**：[03-W25Q16外部Flash与用户存储.md](./03-W25Q16外部Flash与用户存储.md)  
**下一课**：[05-电容触摸FT6336.md](./05-电容触摸FT6336.md)
