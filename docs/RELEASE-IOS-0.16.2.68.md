# iOS 0.16.2.68

配套固件 `7.12.10-ui.2-zelda40.9-musicA.75`。基线 0.16.2.67 / musicA.74。
本版为 **FO-225b 方案 B「封面流 Cover Flow」** 的配套包：App 侧新增 3 张真封面预下发与第 8 个音乐样式，固件侧新增主题 7 与卡带手势。安卓端本版不改。两份 release 保持 draft，不 publish。

## 变更

- **新增 `CoverQueueBridge.swift`**（已手工加入 `project.pbxproj` 四处：PBXBuildFile / PBXFileReference / group children / Sources）：
  - 维护最近 4 张 64² RGB565 缩略封面历史，按曲目 key 去重；每次 `CoverBridge.push()` 成功推当前封后自动刷新队列。
  - bulk **type 12**：`'C''F''Q' ver=1 count` + `count × { slot, zlen(u16 LE), zlib(64² RGB565 LE) }`，slot `0=上一首 / 1=当前 / 2=下一首候选`。
  - 封面来源：slot 1 为当前真封，slot 0 取上一首历史，slot 2 取更早历史；历史不足时取本地资料库两张**不同专辑**的真封面（离线、只查一次并缓存）；一张都没有时才退回「镜像 + 冷青偏色」占位。
  - zlib 用系统 `Compression` 框架真压缩（约 3~5 KB/张），框架不可用或压缩失败自动退回 `CoverPacket.zlibStore()` 的 store 流（固件 tinfl 两种都能解）。`#if canImport(Compression)` 保护，避免可选框架导致编译失败。
  - 只在切歌 / 首次进入音乐页 / 重连时发，**不在滑动时实时取**；BLE 重连由 `BleLink.onLinkReady()` 补发，切到主题 7 时 `Settings.syncMusicStyle()` 也会补一份。
- `Settings.swift`：音乐样式下拉框追加第 8 项 **「封面流 Cover Flow」**（index 7），`musicStyle` 的 clamp 由 6 放宽到 7；`#MS` 协议与纯文字下拉（无缩略图）口径不变。
- `CoverBridge.swift` / `BleLink.swift`：各加一行接入队列桥（推封面后 `accept()`、连上后 `resend()`）。
- `project.pbxproj`：`MARKETING_VERSION` 4 处 → `0.16.2.68`，`CURRENT_PROJECT_VERSION` 4 处 → `77`；新 Swift 文件四处注册。

## 明确没有做的事

- **不加 App 特效开关**：沿用现有「播放风格」下拉框选主题 7，不新增「封面流动效 / 惯性 / 卡数」等开关（沿用用户既有拍板口径）。
- **不做真实播放队列**：iOS AMS 只暴露当前曲目，拿不到「下一首」封面。slot 2 是最近播放 / 本地曲库的真专辑封面候选，不是系统播放队列顺序；手机真的切歌后 AMS 推新封面并重发队列，圆屏自动对齐。想拿真队列需要 MusicKit 用户授权与 Apple Music 订阅，本版不引入。
- **不改开机动画下拉框、不放风格缩略图**；不动 iOS 侧主题 0..6 的任何既有行为。
- 不新增资源位图、不改 IPA 与固件之外的协议；type 5 当前封面流程一字不改。

## 配合固件

- FW musicA.75 新增主题 7：3 张真封的逐列 16.16 定点梯形透视 + 倒影 + 圆屏裁剪，7 卡位循环；卡带 `y 88..286` 横拖驱动相位，12~16 px 阈值，只刷 `318×220 @(74,76)` 局部脏矩形，松手 7 帧 ease-out 按格吸附、无惯性甩动，静止零重绘。
- 吸附完成会真的发 N/P（HID / AMS / legacy 同 `musicTap`），本地队列先滚一格，App 重发队列后归位；歌名文字仍由手机位图下发，切换瞬间有短暂空窗。
- **必须同升 FW musicA.75**：只升 App 会多出一个选不到的下拉项；只刷固件则圆屏收不到 type 12，主题 7 退回 3 张渐变占位卡（能显示、不崩，但不是 3 张真专辑）。

## 建议实机验证

1. 侧载本 IPA + 刷 FW musicA.75，「播放风格」下拉框共 8 项、最后一项是「封面流 Cover Flow」，圆屏回执 `MS,7`。
2. 播一首歌后抓固件日志 `[CFQ] slots=3 ok=111 shift=0`；切歌后队列能刷新，三张侧卡确实是不同专辑。
3. 在卡片带内横拖、松手吸附，确认卡片连续换位、无跳帧顿挫（若装车旋转过角度，`fastPushRect()` 会退化成整屏约 12 fps）；卡带外滑动仍回主页，短拖不误触播放。
4. 重连蓝牙后不重新切歌也能看到 3 张卡（重连补发生效）；切回主题 0..6 不受影响。
