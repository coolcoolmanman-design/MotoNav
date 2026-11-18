# MotoNav 固件 musicA.104 · 全屏转动稳态帧率（P0）

> 配套 iOS **0.16.2.95**。基线 musicA.103。Draft-only。  
> 目标：**转动起来流畅（稳态）**，不追求与黑胶同像素预算。

## 问题

musicA.103 后全屏仍目测卡：每帧 ~16 万像素旋采样冲 28fps 易超时；且每 500ms `musicDirty` 全量重画造成尖峰。见分析 `docs/fullscreen-vs-vinyl-spin-fps-analysis.md`（store）。

## 本版（仅 style 5）

1. **稳态节拍**：全屏旋帧门限 **60ms ≈ 16.7fps**（原 36ms）；角仍按真实 `dt` 积分，避免掉帧大跳角。style 0/6 仍 36ms。
2. **进度局部**：进度条不进 chrome；旋帧轻量重画进度。500ms 刷新改 `ms5ProgDirty`，**禁止**再触发全量 `musicDirty`。

协议 / `COVER_HI=452` / chrome blit 结构保持 musicA.103。

## 刷机

1. 分区表 `0x8000` → app `0x10000` 刷 musicA.104  
2. 装 IPA **0.16.2.95**  
3. 圆屏「全屏转动封面」播放中目测是否更稳（不必与黑胶同细腻）
