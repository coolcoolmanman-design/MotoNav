# Firmware musicA.132 — 矢量3D鸟瞰

相对 musicA.131：

- 新增地图样式 **id 15「矢量3D鸟瞰」**（`ST_VECTOR_3D_BIRDSEYE`），`THEME_N` 15→16。
- 白地 / 浅灰路网、≈55° tilt、沿路网偏置挤出灰描边楼块。
- 路线 / 车头 / 目的地复用 GTA chrome（粉虚线、白三角、粉盘+青准星）；不改 GTA6(14) / 矢量简图(13)。
- 与矢量简图同走 `drawVectorHeroFrame`；路口放大图自动隐藏。

协议：`#t,15,1`；老固件 `t >= THEME_N` 静默忽略。
