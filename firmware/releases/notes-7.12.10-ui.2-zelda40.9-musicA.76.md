# 固件 7.12.10-ui.2-zelda40.9-musicA.76

基线 musicA.75。配套 iOS **0.16.2.69**，两份 release 都保持 draft，不 publish。
修复 FO-225b 上板后的两个实机问题：**① Cover Flow 七张卡全是同一张封面；② 封面背景「放大虚化 / 色块马赛克 / 双色海报」四个档位看不出效果**。

## 问题一：所有封面都一样

抓开机日志确认：App 已连（`TIME SYNC` / `MEDIA HID` / `AMS` / `LYRICS` / `[BG] style=7` 全有），但**一次 `[CFQ]` 都没有** —— bulk type 12 队列从未成功落地，`cfSlotOk[]` 全 false。此时 `cfSrcFor()` 的兜底把 **7 个卡位统统指向同一张 `coverImg`**，视觉上就是「所有封面都一样」。

三条修复：

- **补齐初始化**：`mapInit()` 的 `for (t=1..11)` 漏了 `bulks[12]`，改成 `1..12`，type 12 的 `id/done` 不再是零值。
- **放宽解析门闸并加诊断**：`parseCoverQueue()` 去掉 `zlen <= CFQ_RAW+64` 上限（真 zlib 对复杂封面会膨胀）与 `inBytes == zlen` 强校验（tinfl 可在读完 adler32 前就 DONE），只认 `TINFL_STATUS_DONE + 8192 B`。现在收包、每个 slot 成功 / 失败、整包结果都会打 `[CFQ] rx / slot=N ok / slot=N FAIL / bad zlen`，不再静默丢弃整个队列。
- **兜底不再同图**：新增 `cfBuildFallback()`，在 type 12 未到位但 type 5 到位时，从当前封派生 3 张**不同裁切窗口 + 冷青 / 暖紫 / 原色**的变体（3×8 KB PSRAM，按 `coverSpinAt` 换封面时才重建）；连 type 5 都没有时仍是 3 张不同色调的渐变占位。任何情况下都不会再出现 7 张同图。

## 问题二：封面背景虚化 / 马赛克无效

`MusicBg.h` 的 `musicBgBlit()` 原先要求 `coverValid`（即 type 5 当前封）才生成风格化背景。HID/AMS 模式下第三方播放器经常拿不到本地封面，`coverValid=false` → 直接退回纯色氛围底，四个档位自然全部「无效」。

- 新增 `mbgPickSrc()`：优先 `coverImg`（128²），**没有时改用 type 12 队列的 slot 1 当前封（64²）**；`musicBgBuild()` 不再硬编码 `coverImg`，网格采样走 `mbgSrcPixels / mbgSrcW / mbgSrcH`。
- `parseCoverQueue()` 成功时置 `musicBgDirty`，队列换封后背景立即重建；`#MBG` 换档位时打 `[MBG] build mode=N src=WxH`，便于实机确认到底用了哪张封。
- `musicBgBlit()` 放宽为「档位非 0 + 有任一封面源」即可；暂时无源时沿用上次生成好的 `musicBgBuf`，不会闪回纯色。

## 其余不变

主题 7 的几何、逐列 16.16 梯形采样、倒影、318×220 局部脏矩形、卡带横拖 / ease-out 吸附、`#MS 0..7`、开机 NVS 取模全部沿用 musicA.75，没有回归性改动。不新增 Flash 位图。

## 编译与体积

本地 `python firmware/build.py --cli ... --jobs 8`：**Flash 2,950,231 B（93%）/ RAM 145,468 B（44%）**，相对 musicA.75（2,949,391 B / 145,444 B）只增加 840 B / 24 B，Flash 余量仍约 195 KB。多出的 24 KB 变体缓冲与 24 KB 队列缓冲都在 PSRAM。

## 建议实机验证

1. 刷本版 + 侧载 0.16.2.69，选「封面流 Cover Flow」；开机会看 `[CFQ] rx len=... count=3` → `[CFQ] slot=0/1/2 ok zlen=...` → `[CFQ] slots=3 ok=111 shift=0`。
2. 静止画面中央 / 左右应是三张明显不同的封面（真封或冷暖变体），不再是同一张。
3. 「封面背景」切到「放大虚化 / 色块马赛克 / 双色海报」，日志出现 `[MBG] build mode=1/2/3 src=...`，圆屏背景随之变化；切「关闭」回到纯色底。
4. 切回 style 0..6、导航 / 屏保 / 动态背景 style 6/7 无回归；重连蓝牙不切歌也能看到 3 张卡。
