# musicA.137 — 鸟瞰远端 HUD 裁切

基线 musicA.136（密楼均衡 / 白 HUD / 路口 inset / 无「3D鸟瞰画质」UI / 「换一块圆屏」）。

## 本包

- **导航蓝线**：贴地画到楼块/地网平面尽头再停（插值截断）；地平线以上 / 天空不画，避免飘空横条。
- **红绿灯**：地平面 frustum 外 / 地平线以上不画；落地灯保留去重叠；鸟瞰每路口锚点 ≤2。
- **未改**：iOS 协议/设置（FW-only）；画质仍 bake 均衡。

对照 mock：`docs/birdseye-far-cull-mockup.md` · 实机漂空 `media/birdseye-device-feedback/device-3d-float-far-hud.jpg`。
