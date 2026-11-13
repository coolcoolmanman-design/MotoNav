# iOS 0.16.2.69

配套固件 `7.12.10-ui.2-zelda40.9-musicA.76`。基线 0.16.2.68 / musicA.75。
修复 FO-225b 上板后「Cover Flow 所有封面都一样」的 App 侧根因，并让 HID/AMS 模式下的封面背景档位真正生效。两份 release 保持 draft，不 publish。安卓端本版不改。

## 根因

抓固件开机日志：App 已连（`TIME SYNC` / `MEDIA HID` / `AMS` / `LYRICS` 全有），但**一次 `[CFQ]` 都没有** —— bulk type 12 从未落地。原因是 `CoverQueueBridge.accept()` 只挂在 `CoverBridge.push()` 里，而 `CoverBridge.update()` 在「歌曲没变」时会因 `dedupe == lastKey && lastPushedArt` 直接 return。BLE 重连 / 定时刷新 / 首次进音乐页恰好都命中这条早退，队列桥拿不到任何封面，`history` 为空，`resend()` 也无从补发；固件只能把 7 个卡位全指向同一张 `coverImg`。

## 变更

- `CoverBridge.swift`：`update()` 拿到本地封面后，**在去重早退之前**先调 `CoverQueueBridge.shared.accept(image:key:)`；桥内部按 key 去重，AMS 每几秒一次刷新不会重复发约 18 KB 的队列包。
- `CoverQueueBridge.swift`：
  - `accept()` 内部按 `history.last?.key` 去重；同一首且已发过只更新像素。
  - `send()` **先组好包并缓存 `lastPacket`，再判断 `BleLink.isReady`** —— BLE 尚未就绪时包也留着，`onLinkReady()` 的 `resend()` 能补发。
  - 左右候选不再两次调用同一个 `tinted()`：改成 `variant(seed:)` 两种明显不同的变体（不同镜像方向 + 冷青 / 暖紫），保证首曲、本地曲库为空时三张卡也不会看起来一样。
  - `libraryArts()` 优先按 `MPMediaQuery.albums()` 专辑集合取两张不同专辑真封（HID/AMS 模式下本地单曲常常为空），再用 songs 兜底；结果仍只缓存一次。
  - `zlib()` 增加 `maxZlen = 64²×2+60` 上限：真压缩膨胀过大就退回 `CoverPacket.zlibStore()`，避免整包被固件拒收。
- `project.pbxproj`：`MARKETING_VERSION` 4 处 → `0.16.2.69`，`CURRENT_PROJECT_VERSION` 4 处 → `78`。

## 明确没有做的事

- **不引入 MusicKit / 不申请 Apple Music 订阅**：真播放队列只在 App 自有播放器（`ApplicationMusicPlayer.queue.entries`）可读，会把 App 从「镜子」变成「播放器」并废掉 HID/AMS 主用法。slot 2 仍是「最近播放 / 本地曲库 / 变体」的候选，不是系统 up-next。
- 不加 App 特效开关、不改开机动画下拉框、不放风格缩略图；主题 0..6 与 type 5 当前封面流程一字不改。
- 不新增资源位图、不改协议字段。

## 配合固件

- FW musicA.76 补齐 `bulks[12]` 初始化、放宽 type 12 解析门闸并加 `[CFQ]` 诊断日志；type 12 未到位时从当前封派生 3 张冷暖变体，不再七张同图。
- `MusicBg.h` 在 `coverValid=false` 时改用队列 slot 1 当前封作背景源，封面背景「放大虚化 / 色块马赛克 / 双色海报」四个档位在 HID/AMS 模式下也能生效。
- **必须同升 FW musicA.76**：只升 App 会多一个选不到的下拉项；只刷固件则队列仍可能收不到。

## 建议实机验证

1. 侧载本 IPA + 刷 FW musicA.76，播一首歌抓日志 `[CFQ] rx len=... count=3` → `[CFQ] slot=0/1/2 ok` → `[CFQ] slots=3 ok=111 shift=0`。
2. 三张侧卡明显不同；切换「封面背景」四档，日志 `[MBG] build mode=N src=WxH` 且圆屏背景跟着变。
3. 重连蓝牙、不切歌也能看到 3 张卡；切回主题 0..6 无回归。
