# MotoNav 固件 musicA.82 · 申花开机抗锯齿

> 配套 iOS **0.16.2.76**。基线 musicA.81（申花中央 v2）。Draft 保持。

## 变更

- `ShenhuaBootFrames.h`：9×**466×466** zlib（软边 alpha + 14 色量化）；去掉 233² 半分辨。
- `BootAnimation::renderShenhua`：1:1 inflate 进 FB（不再 `x>>1` 最近邻放大）。
- 中央 idle/connected、等红灯 `#RZ` 未改。

## 刷机

`…_panel0_app_0x10000.bin`（保留 NVS）。
