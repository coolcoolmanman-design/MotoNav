# iOS 0.16.2.51

配套固件：沿用 `7.12.10-ui.2-zelda40.9-musicA.53` 版本号（无固件逻辑变更；CDN 未出 53 bins 时可刷 **musicA.52**）。

基线：0.16.2.50（确认稿烤帧，实机预览倒置且过小）。

## 1. 组合双态预览正立 + 放大

- **方向**：`CenterSpritePixelizer` 加载烤帧时对 CGContext **翻 Y**，RGB565 / 预览均为顶左行序（车轮朝下）；推送 CSP 同帧。
- **尺寸**：按 content bbox 放大贴满中央安全盒（目标宽 ≈320px），消除预览圆内细条大黑边。
- 预览圆直径 88→132；`interpolation(.none)`。

## 2. 无固件改动

CSP 通路不变；勿为本次硬打 FW-only（CDN 504）。
