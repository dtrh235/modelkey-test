# Joyful Zone App 原型

智能门禁 Android App 交互原型，与 Host 端 LVGL 门锁屏共用宜春学院品牌色。

## 预览

在浏览器中打开 `index.html`，可浏览全部页面并跳转交互。

## 打包成 Android APK（Capacitor）

见 **[mobile/BUILD_APK.md](./mobile/BUILD_APK.md)**：安装 Node + Android Studio → `npm install` → `npx cap add android` → Android Studio 打 APK。

## 页面清单

| 文件 | 说明 |
|------|------|
| `index.html` | 原型索引（每行两个模块预览） |
| `splash.html` | 启动页 |
| `onboarding.html` | 新手引导 |
| `login.html` | 登录 / 绑定 |
| `home.html` | 首页（锁态圆钮 + 快捷入口） |
| `unlock.html` | 远程开锁（长按 2 秒） |
| `records.html` | 开锁记录 |
| `devices.html` | 设备管理（Host + Client） |
| `users.html` | 用户管理 |
| `device_detail.html` | 设备详情（子设置入口） |
| `wifi_settings.html` 等 | 设备子页（WiFi / Host / MQTT / 绑定） |
| `notifications.html` | 通知中心 |
| `profile.html` | 个人中心 |

## 设计规范

- 主色：青绿 `#009B8E`（与校徽一致）
- 辅色：学院蓝 `#1A6BB5`、琥珀金 `#FFB800`、旭日橙 `#E85D04`
- 技术栈：HTML5 + Tailwind CSS + Font Awesome + `shared.js`

## LVGL 对应

Host 工程 `app_ui_theme.*` / `app_ui_redesign.*` 已按同一色系重做门锁屏布局。

## 导航层级（返回键）

```
首页 home
  └─ 设备管理 devices  ← 底栏「设备」
       └─ 设备详情 device_detail  ← 点击「实验室智能门锁」
            ├─ Host / Client / WiFi / App绑定 / MQTT
            └─ 远程开锁 unlock → 返回详情或首页
  └─ 开锁记录 / 用户 / 通知 / 我的（各模块独立返回首页）
```

门锁插图使用 `shared.js` 内嵌 SVG，不依赖外网图片。
