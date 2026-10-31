# MotoNav iOS 0.15.27-tpms

通用圆屏固件配套 · 基于 `0.15.26-tpms`：**修复导航/模拟导航打开即闪退**；App 任意页面 BLE 自动连上时弹出连接动画。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### P0 · 导航 / 模拟导航闪退
- 根因：`AMapNaviDriveView.trackingMode` setter 会同步回调 `didChangeTrackingMode:`；0.15.26 的 `reassertNavCamera` 在回调里无条件再写 `trackingMode = .carNorth`，形成重入 → 栈溢出闪退。
- 修复：写入门闩 + 仅在值不同时设置；`addDataRepresentative` 后再挂 delegate / 应用车头向上与回锁车；光柱只在关闭时重开。
- 保留 UX：半透明 HUD、车头向上、光柱、约 7s 自动回跟车。

### 自动连接动画
- 监听 `BleLink.isReady`：任意路径（启动自动连 / 断线重连 / 手动）链路就绪时弹出同款 ConnectSheet / ConnectPuck 动画。
- 已在展示中不叠第二层；每个连接周期只庆祝一次。

### 独立导航 MotorMap（稳妥）
- overlays / annotations 空安全；航向改用 `rotationDegree` 属性赋值。

### 继承 0.15.26（未改意图）
- 非机动车途经点、底栏毛玻璃、「语音」文案。

## 固件

本版 **不改固件**。请同时刷入现网配套 **`7.12.11-opt.9`**（或更新）。勿与 stopwatch / flag 协议包交叉。

## 安装

- IPA：`MotoNav-0.15.27-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
