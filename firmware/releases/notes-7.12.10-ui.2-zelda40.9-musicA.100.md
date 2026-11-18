# MotoNav 固件 v7.12.10-ui.2-zelda40.9-musicA.100

> 配套 iOS：0.16.2.91  
> 面板：1.75 圆屏（466×466）

## 本版变更

### 申花中央动画（idle + connected）soft_keep 重 bake
- 源：`user-ref-100pct-v4-clean-sized/idle-189` + `connected-189`（clean-then-NEAREST）。
- bake：`soft_keep_valid`（ALPHA_KEEP=24）→ harden α=255；`pack565` 将有效像素 RGB565 `0→1`，防 chroma 透底。
- 足迹仍 **189×135**（屏上 2× ≈378×270）；**不含体育场**（stadium 仍只在 dynBg style 8）。
- bleed：`MAX_SEE_THROUGH=0`。

### 保持
- 无 SoftAP / WiFi 地图加速（musicA.99 起移除）。
- 4MB app 分区表；刷机先 `0x8000` 分区表再 `0x10000` app。

## 升级提示
- 与 iOS 0.16.2.91 同测：预取加长 + 申花干净中央动画。
