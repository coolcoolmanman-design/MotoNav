# iOS 0.16.2.53

配套固件 `7.12.10-ui.2-zelda40.9-musicA.55`。基线 0.16.2.52。

## 组合动画：从「推送 CSP」改为「发索引」

- `CenterSpriteBridge` 重写：16 组合已烧进固件，`setGender` / `setVehicle` 直接发
  `#CS,<gender>,<vehicle>`（2 字节），**选择即生效、零 BLE bulk、零等待**；
  `pushAll()` 同步索引但不改 `vehId`，不会覆盖用户的塞尔达 / GTA 载具选择。
- 预览直接读 `Assets.xcassets` 里重烘焙的正立 PNG，与圆屏同源；
  移除运行时像素化、`CG` 翻 Y、整帧 bob —— 修掉「App 预览倒的、推送到圆屏也是倒的」
  与「整车上下弹」两个问题。
- `MotoNavApp.swift` 分发 `CS,g,v` 回执；设置页文案改为
  「固件内置 · 16 组合 · 选择即生效 · 无需 BLE 推送」，按钮「应用到圆屏」，
  「清除组合」改「恢复默认组合」（男生 × 摩托车）。
- 保留 `noteDeviceCached()` / `resendIfPossible()` 入口（重连时只重发索引），
  不再清 ffat、不再发 bulk type 11。

## 素材重烘焙（与固件同源）

`scripts/bake-combo-builtin.py` 覆盖写回 32 张 `Combo_*.png`：
抠掉两轮车的深蓝黑面板与暗圆环、去掉残留文字、按车型放大到统一目标框，
sleep/wave 共用并集 bbox。修掉实机「摩托 / 踏板摩托 / 竞速摩托 / 自行车 /
折叠小轮车尺寸小且带黑背景」以及「连接态动画位置太靠上」。

## 矢量简图

本版 iOS 无改动（真实路线数据一直在发），修复全在固件端。
