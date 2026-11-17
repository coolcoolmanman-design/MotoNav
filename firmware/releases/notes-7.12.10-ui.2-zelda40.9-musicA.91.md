# MotoNav 固件 musicA.91 · 申花 conn-v2 bake

> 配套 iOS **0.16.2.85**。基线 musicA.90。**musicA.84 仍不安全，勿刷。** Draft 保持。

## 本版

- **Idle / Connected：** 确认的 conn-v2 mockup（黑/透明底）；**189×135**；硬 body α（LANCZOS 软缘丢弃，无 soft bleed on dynBg）。
- **Connected：** idle 盾徽合进车身；去掉 flag/车身硬贴白圆标。
- **体育场：** 仍仅 dynBg style 8（不进连接态）。

## Flash

- Sketch `3120783` · app.bin `3121040` · 余量 **24688 B**（≈24.1 KiB ≥1.5 KiB）。

## 透底（bake 后）

见 store `bleed-metrics-musicA91.txt`（soft→see_through 已丢弃；valid α=255）。

## 版本

- FW `7.12.10-ui.2-zelda40.9-musicA.91`
- iOS `0.16.2.85`
