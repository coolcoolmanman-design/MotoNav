# iOS 0.16.2.23（路口图验证 + 屏保开关）

## 一、路口放大图验证（0.16.2.22 起）

目标不是立刻推到圆屏，而是先看清高德给的图长什么样，再决定怎么压。

- 接 `driveManager:showCrossImage:` / `driveManagerHideCrossImage:`
- 导航页顶部「路口图预览」卡：原图 + `#序号 宽×高px 比例 是否含透明 PNG 体积`，标题可折叠
- 图落地 `Caches/cross-captures/cross-XXX.png`（保留最近 12 张）
- `image` 为 nil 时明确提示（10.0.2 起自定义 View 里 `showCrossImage = YES` 会这样）

## 二、灯态（红绿灯倒计时）接口探测

- 开导航时探一次并把结论写到「设置 → 调试信息 → 灯态API」和 `Caches/signal-probe.txt`：
  - `AMapNaviTrafficLight` / `AMapNaviTrafficLightInfo` / `AMapNaviSignalLight` / `AMapNaviCross` 是否存在、有哪些属性
  - `AMapNaviDriveDataRepresentable` 里有没有 `updateTrafficLights:` 之类方法
- 同时挂了 `driveManager:updateTrafficLights:` 实现：SDK 真有才回调，调到了就把灯态对象逐字段打出来；没有就是死代码

## 三、新增「屏幕保护」开关

「圆屏 Tab → 圆屏显示」新增开关，默认开：

- 开：待机页静置 60 秒进入眼睛动画屏保（呼吸/眨眼/扫视 + IMU 互动）
- 关：一直停在待机页；如果正在屏保会立刻退出
- 开关状态存手机（`saver_on`），并下发 `#SV,1` / `#SV,0` 给圆屏，圆屏自己存 NVS，重连时自动重发
- **需要固件 musicA.25 或更新版**（旧固件不认识 `#SV`，会忽略）

## 安装

unsigned ipa，照常 iLoader 侧载。建议同刷固件 musicA.25，否则屏保开关不生效。
