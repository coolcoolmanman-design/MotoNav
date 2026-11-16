# MotoNav 固件 musicA.80 · 申花动画 + 等红灯微远推

> 配套 iOS **0.16.2.74**。基线 musicA.79 / iOS 0.16.2.73（含 Duo/TPMS 闪退修复）。
> Release 保持 Draft。

## 等红灯自动缩放（Option A · 微远推）

- `#RZ,0/1` + NVS `rl_zoom`（默认关）。
- `drawMapFrame`：`inNav` 会话 + 开关开时，`posKmh<2.5` 持续 3 s 进入等待态；`viewScale` 在停车近端与微远端（−0.08）之间 10 s 正弦呼吸；蓝图/罗盘/龙主题网格跳过；`posKmh>4` 立刻退出交回车速公式（τ≈1.2 s）。

## 申花主题动画

- 开机品牌 **19 = SHENHUA**（旧 Off 19 → 20）；`ShenhuaBootFrames.h` 9×233² zlib，2× blit。
- 中央载具 **vehId=5**：`ShenhuaCenterIdle.h` / `ShenhuaCenterConn.h` 各 8×210×150 chroma 帧，锚点 (233,248)。
- Flash：三套合计 ≈151 KiB zlib（app 槽 3 MiB 内）。

## 刷机

优先 `…_panel0_app_0x10000.bin`（保留 NVS）。首次升级后若曾选「关闭开机动画」，固件/App 会一次性把旧 ID 19 迁到 20。
