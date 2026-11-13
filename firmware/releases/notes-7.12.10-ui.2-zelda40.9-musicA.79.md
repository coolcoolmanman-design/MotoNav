# MotoNav 固件 musicA.79 · FO-225g 自动切歌卡组滑动

> 圆屏音乐「封面流」主题（style 7，Cover Flow）的体验增强：**手机自动跳到下一首时，卡组播放
> 一次前进吸附动画**，而不是瞬间硬切封面。固件 + iOS 双端改动，iOS 升至 **0.16.2.71**。
> 所有 Release 一律 draft（published_at=None）。

## 现象（实机反馈）
此前自动切歌（曲目播放完 App 自动下一首 / 系统播控跳下一首）时，手机重发 type 12 队列，
`parseCoverQueue()` 无条件 `cfShift=0; cfPhase=0`，封面直接**硬跳**到新当前封——没有 FO-225f
给手动点按钮做的那一下「卡组滑过一格」的过渡，视觉上像闪了一下。

## 根因
`parseCoverQueue()` 收到队列后无论来源一律硬归位，没有区分「本机手动点按（已本地滚过一格）」
与「手机主动切歌（卡组还停在旧当前封）」。后者同样需要一次滑动动画，但原逻辑没有触发入口。

## 修复
- `DeviceContext.h` 新增缓冲与标志：`cfPending[3]` / `cfPendingSlotOk[3]`（自动切歌期间解码的新封面
  缓冲）、`cfPendingReady`、`cfAdvanceAuto`（本次吸附是手机自动切歌，收尾不回发切歌命令）、
  `cfSelfAdvance` / `cfSelfAdvanceAt`（本机点按/手势发过命令，2s 内忽略手机回环的 #CFN）。
- `CommandReceiver.h` 新增 `#CFN` 指令：主题 7 静止时调用 `cfStartSnap(1)` 起一次前进吸附，并置
  `cfAdvanceAuto=true`；`cfSelfAdvance` 的 2s 回环窗口内忽略，避免与手动点 next 的吸附重复触发。
- `RasterDecoder.h` `parseCoverQueue()`：当 `cfAdvanceAuto && cfSnap` 时，把新封面解进 `cfPending`
  缓冲并置 `cfPendingReady`，**不重置 phase/shift**；否则保持原硬归位行为。
- `MusicThemes.h` `cfSnapTick()`：吸附收尾若 `cfAdvanceAuto`，一次性把 `cfPending`（仅 OK 槽）提交到
  `cfCovers`（旧缓冲回收为下一轮 pending，零泄漏），`musicBgDirty=true` 重建背景，且**不发切歌命令**；
  手动点按路径收尾时置 `cfSelfAdvance` 标记，回环防抖。

## 行为对照
| 场景 | 修复前 | 修复后 |
|---|---|---|
| 手机自动跳下一首（主题 7） | 封面硬跳 | 卡组滑过一格（≈315ms ease-out）后落定新当前封 |
| 手动点 prev/next 按钮 | 本地滚一格 + 吸附收尾发命令 | 不变（仍本地滚 + 发命令） |
| 手动拖动卡带 | 跟手 + 吸附 | 不变 |
| HID/AMS 下自动切歌 | 封面硬跳 | 卡组滑一下（手机重发队列后落定） |

## 协议
- 新增文本指令 `#CFN`：手机在真实切歌（仅主题 7）且 BLE 就绪时，先于 type 12 队列下发，触发一次
  前进吸附。无 ACK（沿用 `#` 指令忽略未知字眼的既有约定，无需回执）。
- type 12（CFQ）协议字段不变；`cfPending` 仅固件内部缓冲，不新增 BLE 负载。

## 发布与刷机
- CI workflow：固件 `release-firmware.yml` 与 iOS 双触发（App/协议均改）。
- 编译体积：待 `python firmware/build.py --cli ... --jobs 8` 出包后补（预期增量极小，< 1 KB Flash）。
- 交付：固件 draft（untagged），官方 `app.bin` 校验 SHA256 一致后写 COM5 0x10000，不擦 NVS。
- 开机日志预期：`[CFQ] #CFN auto-advance` → `[CFQ] buffered auto-advance slots ok=111` →
  `[CFQ] auto-advance committed ready=1`。

## 已知边界
- 若 type 12 队列在吸附动画（≈315ms）结束之后才到（如 iTunes 异步取封较慢），`cfPendingReady` 为假，
  吸附收尾直接复位、随后队列按原硬归位落定——仍有滑动过程，只是末尾不再做缓冲提交；属于容错回退。
- 本地轮播/随机下一首语义同手动：只要手机重发队列，卡组最终以手机为准。
