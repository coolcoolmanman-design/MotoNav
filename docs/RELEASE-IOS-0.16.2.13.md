# MotoNav iOS 0.16.2.13 (build 28)

在 0.16.2.12 基础上（Draft-only · 脱敏上传 · 无 PR）：

## 修复：到达庆祝不再自动消失

- **现象**：到达后圆屏格子旗 + 里程/时长 + OK，约数秒后自动关掉。
- **根因**：圆屏固件自 zelda40.3/40.6 起已是「常驻等 OK」；iOS 到达后仍定时发 `C` 清屏，把庆祝画面抢关。
- **改动**：
  - `NaviBridge.finish`：去掉到达后 4s 自动 `C`；到达卡常驻到用户点 **OK**
  - `NaviScreen.arrivedCard`：显式 OK 按钮（不再“马上回到主界面”点空白即关的定时语义）
  - `WebMotor`：独立导航同样不再 4s 自动 `C`
- **配套固件**：请刷 **musicA.12**（`C` 在目的地庆祝期间忽略抢关）。仅升 IPA 也能改善；双升最稳。

## 修复：连接动画地面影过大

- 对照 `device-shadow-proportion-target-2026-11-03.jpg`
- `ConnectPuckSceneView` 软椭圆平面：`2.55×1.05` → **`0.90×0.26`**（接触核约设备直径 40%、浅扁）
- 程序化椭圆纹理收紧羽化；保持磨砂银壳 + 软椭圆（无方影、无中环）

## 配套固件

`7.12.10-ui.2-zelda40.9-musicA.12`（到达 OK 不被 App `C` 抢关）。本 IPA CI 仍会附带分支内固件产物；**夜刷请以 musicA.12 Firmware Draft 为准**。
