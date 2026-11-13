# iOS 0.16.2.71

配套固件 `7.12.10-ui.2-zelda40.9-musicA.79`（**固件需同步刷**）。基线 0.16.2.70 / musicA.78。
为「封面流」主题（style 7）的**自动切歌滑动动画**补上 App 侧触发：真实切歌时先发 `#CFN` 让圆屏卡组
滑一下。两份 release 保持 draft，不 publish。安卓端本版不改。

## 变更

- `CoverBridge.swift` `update(title:artist:item:)`：在真正切歌（track key 与上一次不同）且当前是
  封面流（`Settings.shared.musicStyle == 7`）、且 BLE 已就绪时，先于 type 12 队列下发文本指令
  `#CFN`，触发圆屏一次前进吸附；用 `lastCFNKey` 去重，重连/补发同曲不再重复起滑。
  - `reset()` 与 `clearOnScreen()` 一并清空 `lastCFNKey`，停止播放 / 重连重置后下一首仍能触发。
- `CoverQueueBridge.swift` 不变：仍是 `accept()` → `send()` 组 64² RGB565 → `CoverPacket.zlibStore()`
  → type 12 队列，收到 `#CFN` 后紧随其下发。
- `project.pbxproj`：`MARKETING_VERSION` 4 处 → `0.16.2.71`，`CURRENT_PROJECT_VERSION` 4 处 → `80`。

## 配合固件

- 固件 musicA.79 新增 `#CFN` 处理：`cfStartSnap(1)` 起前进吸附 + `cfAdvanceAuto` 标志；`parseCoverQueue()`
  在自动吸附期间把队列解进 `cfPending` 缓冲，吸附收尾整体提交到 `cfCovers` 并重建背景——做到「滑一下」
  而非硬切。手动点按钮的吸附（`cfSelfAdvance` 2s 回环窗口）不会被 `#CFN` 重复触发。
- 侧载 0.16.2.71 + 刷 musicA.79 后，自动跳下一首时圆屏卡组应播放一次滑动动画再落定新当前封；
  手动点 prev/next 仍为本地滚一格，行为无回归。

## 明确没有做的事

- 不引入 MusicKit / Apple Music 订阅；slot 2 仍是候选封面，不是系统 up-next。
- 不改固件协议字段、不新增资源位图；主题 0..6 与 type 5 封面流程一字不改。
- 不新增 App 开关；`#CFN` 仅在主题 7 下才发，其它主题忽略。

## 建议实机验证

1. 侧载本 IPA 并刷 musicA.79，进入主题 7 播放，让曲目自然播完自动下一首（或播控跳下一首）。
2. 观察圆屏：卡组应滑过一格（而非硬跳）后落定新当前封。
3. 抓日志确认 `[CFQ] #CFN auto-advance` → `auto-advance committed ready=1`。
4. 手动点 prev/next 仍为本地滚一格，且无重复滑动；切回主题 0..6 无回归。
