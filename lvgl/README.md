# JoyfulZone — 门锁 UI 预览 v0.3

纸质文艺浅色风门锁界面，**仅 HTML 预览**，供确认视觉与交互后再移植 LVGL。

## 打开

双击 `index.html` 或用浏览器打开：

```
D:\keil project\modelkey\lvgl\index.html
```

## v0.3 要点

- **首页**：仅大时钟 + 锁孔主视觉 +「请刷卡 / 请按指纹」
- **纸质文艺**：暖米杏纸感、Noto 中文衬线/黑体，背景始终缓慢漂移动效
- **开锁成功**：全屏暖色涟漪 +「门已打开 / 欢迎」短文案
- **触屏**：左滑返回、上下滑列表、首页横滑切换底栏、WiFi 页下拉刷新
- **物理键**：输入框（账号/密码/WiFi 密码）用下方方向键 + 确认；WiFi 密码屏上仅掩码
- **品牌**：配对页展示 JoyfulZone
- **用户列表**：管理员（淡金标签）/ 普通用户（赤陶标签）

## 演示账号

| 场景 | 账号 | 密码 |
|------|------|------|
| 用户开锁 | 20230101 | 1234 |
| 管理员 | admin | admin |
| 查找用户 | 20230215 | — |

首页双击设备屏，或点「感应开锁」模拟 NFC/指纹。

## 文件

```
lvgl/
  index.html
  css/preview.css
  js/preview.js
  js/touch.js
  README.md
```

## 固件移植（进行中）

Host 工程已加入 **UI v3** 预览移植层（`Host/User/ui_v3/`），`app_config.h` 中 `APP_UI_V3_ENABLE=1` 时：

- 使用精简字库 `lv_font_ui_v3_13/20/num_36`（仅 preview 文案，见 `guider/tools/gen_ui_v3_fonts.ps1`）
- 40 行双缓冲 + CCM 显存，刷新周期 16ms，GUI 任务 5ms
- LVGL 原生触摸 + 滑动手势（左滑返回、列表/表单上下滑）
- 首页锁孔呼吸/弧旋转、双击涟漪开锁动效（无真实门锁逻辑）

烧录验证通过后，可在 Keil 的 guider 组 **移除旧 SourceHan/cn_wifi 字库与 setup_scr_*.c** 以节省 Flash。
