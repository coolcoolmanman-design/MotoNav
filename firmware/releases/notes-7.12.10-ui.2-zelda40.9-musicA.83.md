# MotoNav 固件 musicA.83 · 申花中央 FO-229 大图

> 配套 iOS **0.16.2.77**。基线 musicA.82（开机 AA）。Draft 保持。

## 变更

- `ShenhuaCenterIdle.h` / `ShenhuaCenterConn.h`：FO-229 效果图重烘焙（仍 210×150 chroma，zlib +4.5 KiB，3 MiB 余量内）。
- `drawShenhuaCenter`：最近邻 **2×** 铺到约 420×300（锚点 233,248）；接受可能叠左右槽。
- 开机 AA / 等红灯未改。

## 刷机

`…_panel0_app_0x10000.bin`（保留 NVS）。
