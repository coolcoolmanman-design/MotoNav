# MotoNav 固件 musicA.81 · 申花中央动画 v2

> 配套 iOS **0.16.2.75**。基线 musicA.80（申花 v1 + 等红灯微远推）。
> Release 保持 Draft。

## 变更

- 重烘焙 `ShenhuaCenterIdle.h` / `ShenhuaCenterConn.h`（David 确认 v2 效果图）。
- Idle：拳击循环、无盔；Connected：骑行 + 趴地拖绳同朝向合帧。
- Boot `ShenhuaBootFrames.h` 未改；`#RZ` 等红灯缩放未改。
- zlib 合计 idle+conn ≈87 KiB（较 v1 ≈112 KiB 更小，Flash 更宽裕）。

## 刷机

优先 `…_panel0_app_0x10000.bin`（保留 NVS）。
