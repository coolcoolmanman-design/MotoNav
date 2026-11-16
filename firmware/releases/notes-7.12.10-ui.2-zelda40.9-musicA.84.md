# MotoNav 固件 musicA.84 · 龙珠雷达扇面扫动

> 配套 iOS **0.16.2.78**。基线 musicA.83（申花 FO-229 large）。Draft 保持。

## 变更

- theme==11：`drawDragonRadarSweep` — ~70° 扇面 + 亮前缘顺时针扫动（圆心对齐绿盘十字；线扇瘦身以贴 3 MiB）。
- 扫过光点 / 四星球：`drawDragonBallSweepPing` 短暂涟漪高亮。
- 申花 large、开机 AA、等红灯未改。

## 刷机

`…_panel0_app_0x10000.bin`（保留 NVS）。
