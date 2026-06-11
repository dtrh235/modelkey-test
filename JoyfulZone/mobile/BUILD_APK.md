# Joyful Zone — Capacitor 打 Android APK

把 `JoyfulZone/` 网页包进 WebView，生成可安装的 `.apk`。

## 环境准备（只需装一次）

### 1. Node.js
- 安装 [Node.js 18+](https://nodejs.org/)（自带 npm）
- 终端验证：`node -v`、`npm -v`

### 2. Android Studio
- 安装 [Android Studio](https://developer.android.com/studio)
- 首次打开：**More Actions → SDK Manager**，勾选：
  - Android SDK Platform（建议 API 34）
  - Android SDK Build-Tools
- **Settings → Build → Build Tools → Gradle**：使用自带 JDK 17

### 3. 环境变量（可选，命令行打包容错）
- `ANDROID_HOME` = SDK 目录，例如  
  `C:\Users\你的用户名\AppData\Local\Android\Sdk`
- Path 增加：`%ANDROID_HOME%\platform-tools`

---

## 第一次：初始化 Android 工程

在 **本目录** `JoyfulZone/mobile/` 执行：

```powershell
cd "d:\keil project\modelkey\JoyfulZone\mobile"
npm install
npm run prepare:www
npx cap add android
npx cap sync
```

说明：
- `prepare:www` 会把上级 `JoyfulZone/*.html`、`shared.js`、`assets/` 复制到 `www/`
- APK 启动入口为 `splash.html`（不是开发用的页面索引）
- `android/` 目录由 Capacitor 自动生成，**可重复 sync，不要手改 Web 资源路径**

用 Android Studio 打开：

```powershell
npx cap open android
```

---

## 日常：改完网页后重新打包

每次修改 `JoyfulZone/` 里 HTML/JS 后：

```powershell
cd "d:\keil project\modelkey\JoyfulZone\mobile"
npm run cap:sync
npx cap open android
```

在 Android Studio 中：
1. 菜单 **Build → Build Bundle(s) / APK(s) → Build APK(s)**
2. 完成后点 **locate**，得到  
   `android/app/build/outputs/apk/debug/app-debug.apk`

`app-debug.apk` 可直接发给用户 **侧载安装**（需允许「未知来源」）。

---

## 版本号怎么改（上架 / 发新版）

编辑 **`android/app/build.gradle`**（`cap add android` 之后才有）：

```gradle
android {
    defaultConfig {
        applicationId "edu.ycu.joyfulzone"
        minSdkVersion rootProject.ext.minSdkVersion
        targetSdkVersion rootProject.ext.targetSdkVersion
        versionCode 2        // 整数，每次发版 +1（商店强制递增）
        versionName "1.0.1"  // 给用户看的版本号，与 about 页一致即可
    }
}
```

建议同步修改：
- `mobile/package.json` 的 `"version"`
- `JoyfulZone/about.html` 里显示的 `v1.0.x`

改完后重新 **Build APK**；用户安装新 APK 即完成更新（侧载需覆盖安装，数据在 `localStorage` 里会保留）。

---

## 发布正式包（非 debug）

Android Studio：**Build → Generate Signed Bundle / APK**
- 选 **APK**
- 新建或选择 **keystore**（签名密钥，丢失后无法覆盖更新同一包名）
- 选 **release** 构建类型

Release 包体积更小、可上架应用商店。

---

## 真机调试

1. 手机开启 **开发者选项 → USB 调试**
2. USB 连接电脑
3. Android Studio 顶部选你的手机 → 点 **Run ▶**

或命令行：

```powershell
npm run cap:run
```

---

## 手机绑定门锁：后端地址

APK 里的网页会请求电脑上的 **JoyfulZone/server**（默认 `http://192.168.126.244:3000`）。

1. 电脑与手机连 **同一 WiFi**
2. 电脑执行：`cd JoyfulZone/server && npm start`
3. 电脑 IP 可用 `ipconfig` 查看（选 `192.168.x.x`，不要选 `198.18.x` 等虚拟网卡）
4. 若 IP 变了：改 `JoyfulZone/shared.js` 里的 `JZ_DEFAULT_LAN_API_BASE`，或在 App **添加设备** 页填写后端地址 → **测试后端连接**
5. Windows 防火墙需放行 **入站 TCP 3000**（或临时关闭防火墙测试）

---

## 常见问题

| 问题 | 处理 |
|------|------|
| 无法连接后端 | 确认 server 已启动；手机与电脑同 WiFi；添加设备页测试连接；防火墙放行 3000 |
| 页面空白 | 先 `npm run prepare:www` 再 `npx cap sync` |
| 图标/字体不显示 | 需联网（Tailwind / Font Awesome 用 CDN）；或后续改为本地静态资源 |
| 只能看原型索引 | 确认 `www/index.html` 跳转到 `splash.html`，不要直接用 catalog 版 index |
| Gradle 很慢 | Android Studio 设置国内 Maven 镜像，或耐心等待首次下载 |

---

## 目录结构

```
JoyfulZone/
  *.html  shared.js  assets/     ← 你平时改的原型
  mobile/
    package.json
    capacitor.config.json
    scripts/prepare-www.ps1
    www/                          ← 自动生成，勿手改
    android/                      ← Capacitor 生成，用 Android Studio 打开
    BUILD_APK.md                  ← 本文档
```

---

## 与门锁固件版本的关系

| 项目 | 版本位置 |
|------|----------|
| **手机 APK** | `android/app/build.gradle` → `versionCode` / `versionName` |
| **门锁 Host 固件** | 仓库 `CHANGELOG.md`、Git Tag、云端 OTA |

两者独立编号，不要混用同一个版本号。
