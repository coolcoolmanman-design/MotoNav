# MotoNav 固件 v7.12.10-ui.2-zelda40.9-musicA.101

> 配套 iOS：0.16.2.92  
> SoftAP / Wi‑Fi 地图加速：关（沿用无 SoftAP 基线）

### 申花中央动画（idle + connected）user-orig small+bright

- 源：`media/shenhua-redraw-mock/user-orig-small-bright/{idle,connected}-165/`（David 确认；连接态 01–08 全帧趴地红人+绳；红人比例 idle ROTATE_90）
- bake：`soft_keep_valid`（ALPHA_KEEP=24）→ harden α=255；`pack565` 有效像素 RGB565 `0→1`
- 足迹：**165×118** → 屏上 2× **330×236**；无 stadium（dynBg style 8 不变）
- `MAX_SEE_THROUGH=0`（`bleed-metrics-musicA101.txt`）

### 继承

- 预取加长与回前台建筑补采仍在配套 iOS 0.16.2.92（同 0.16.2.91 参数）
- SoftAP 仍关

### 刷机

1. 分区表 `0x8000` → app `0x10000` 刷本 Draft bin
2. app 分区 ≤3145728，留 ≥~1.5 KiB
