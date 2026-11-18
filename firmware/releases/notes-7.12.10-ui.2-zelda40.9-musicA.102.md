# MotoNav 固件 v7.12.10-ui.2-zelda40.9-musicA.102

> 配套 iOS：0.16.2.93  
> SoftAP / Wi‑Fi 地图加速：关

### 龙珠雷达 v3（theme 11）

- 恢复 `DragonRadar::drawGrid`：笛卡尔方格十字，**step=40**，中心十字加粗（对齐 manga 参考与 v3 mock）
- `drawDragonRadarSweep`：亮薄前缘 + ~10° 辉光 + ~55° 指数软拖尾（~2.6s/周）；无 blip/ping（省 Flash）
- 四星球仍用瘦身四点球

### 继承

- 申花中央 user-orig small+bright（musicA.101）
- SoftAP 仍关；预取/混合轮廓在配套 iOS

### 刷机

1. 分区表 `0x8000` → app `0x10000` 刷本 Draft bin
2. CI app 分区方案最大 4 MiB；现场双刷沿用分区表+app
