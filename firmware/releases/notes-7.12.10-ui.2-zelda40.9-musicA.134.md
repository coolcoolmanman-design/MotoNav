# Firmware musicA.134 — Tilt3D 矢量3D鸟瞰

- 合入 David zip：`Tilt3D.h` / `Tilt3DRaster.h` + MapFrame/Route/Markers/Camera hooks。
- **style 15「矢量3D鸟瞰」** 自动开透视（也接受 `#3D,1/0`）；不再走 VectorHero 独立排版。
- 栅格地面 + 索引 4..7 楼块拉伸；自带浅灰白 / 夜深蓝灰配色（对齐 preview day/night）。
- 路线/幽灵/灯/电子眼/终点经 `t3dXY`；车头用 `MAP_CAR_Y`。非 GTA chrome。
