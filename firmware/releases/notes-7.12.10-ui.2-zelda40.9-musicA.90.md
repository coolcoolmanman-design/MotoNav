# MotoNav 固件 musicA.90 · 申花中央拆分 + 体育场 dynBg

> 配套 iOS **0.16.2.84**。基线 musicA.89。**musicA.84 仍不安全，勿刷。** Draft 保持。

## 本版

- **Idle / Connected：** 均用 B hero（黑/透明底骑拖）；同一 bake 画布 **189×135**；soft rim + hard body。
- **体育场：** 不再合进连接态；新增 **动态背景 style 8「申花体育场」**（半分辨 still + 草坪轻滚动）。
- Flash：`Sketch 3140159` · `app.bin 3140416` · 余量 **5312 B**（≈5.2 KiB ≥1.5 KiB）。

## 透底（bake 后）

见 store `bleed-metrics-musicA90.txt`：`MAX_SEE_THROUGH=103`（外缘未纳入 body 的 soft 像素，已置透明）；valid 像素 α 硬化为 255。

## 版本

- FW `7.12.10-ui.2-zelda40.9-musicA.90`
- iOS `0.16.2.84`
