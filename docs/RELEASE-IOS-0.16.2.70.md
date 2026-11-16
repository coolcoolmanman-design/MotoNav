# iOS 0.16.2.70

配套固件 `7.12.10-ui.2-zelda40.9-musicA.76`（**固件不变，已刷在板上**）。基线 0.16.2.69 / musicA.76。
修复 0.16.2.69 仍解不出封面的最后一处根因。两份 release 保持 draft，不 publish。安卓端本版不改。

## 实机日志钉死的根因

刷 musicA.76 后抓开机日志，App 侧队列其实**一直在发**：

```
[CFQ] rx len=17444 count=3
[CFQ] slot=0 FAIL zlen=4930 st=-1 in=2 out=0
[CFQ] slot=1 FAIL zlen=7487 st=-1 in=2 out=0
[CFQ] slot=2 FAIL zlen=5013 st=-1 in=2 out=0
```

`st=-1`（`TINFL_STATUS_FAILED`）、`in=2` 表示 tinfl 读完 2 字节 zlib 头就判定失败 —— Apple `Compression` 框架的 `COMPRESSION_ZLIB` 输出的是 **raw DEFLATE**，不带 RFC1950 头与校验，而固件 `parseCoverQueue()` 用的是 `TINFL_FLAG_PARSE_ZLIB_HEADER`。三个 slot 全部解压失败 → `cfSlotOk[]` 全 false → 7 个卡位回退到同一张 `coverImg`，就是「所有封面都一样」；同时 `cfCovers[1]` 不存在，风格化背景也拿不到封面源。

顺带确认：此前「App 没发队列」的猜测不成立，`resend()` 在重连时确实补发了整包；0.16.2.69 加的去重早退前 `accept()`、先缓存包再判断 `isReady` 属于额外保险，保留。

## 变更

- `CoverQueueBridge.swift`：删除 `Compression` 依赖，`zlib()` 统一返回 `CoverPacket.zlibStore(raw)` 的标准 RFC1950 zlib store 流 —— 与 type 5 当前封面同一套编码，固件 musicA.76 的标准 tinfl 路径 100% 可解。每张 8203 B、整包约 24.6 KB（仍在 `CFQ_MAX=24782` 内），只在切歌 / 首次进音乐页 / 重连发一次，BLE 约 5 秒。
- 其余 App 侧修复沿用 0.16.2.69：去重早退前 `accept()`、先缓存包再判断 `isReady`、两种冷暖色调变体、按专辑集合取本地真封。
- `project.pbxproj`：`MARKETING_VERSION` 4 处 → `0.16.2.70`，`CURRENT_PROJECT_VERSION` 4 处 → `79`。

## 配合固件

- **固件无需再刷**：musicA.76 已包含 `bulks[12]` 初始化、放宽后的 type 12 解析门闸、`[CFQ]` 诊断日志、三张冷暖变体兜底、背景源回退队列 slot 1；本版只补 App 侧编码格式。
- 侧载 0.16.2.70 后，开机日志应变为 `[CFQ] slot=0/1/2 ok zlen=8203` → `[CFQ] slots=3 ok=111 shift=0`；切「封面背景」四档可见 `[MBG] build mode=N src=64x64`（或 `128x128`）。
- 三张真封到位后，「放大虚化 / 色块马赛克 / 双色海报」都会基于当前封重建，HID/AMS 模式下也不再退回纯色底。

## 明确没有做的事

- 不引入 MusicKit / Apple Music 订阅；slot 2 仍是候选封面，不是系统 up-next。
- 不加 App 特效开关、不动开机动画下拉框、不放风格缩略图。
- **不改固件、不改协议字段**、不新增资源位图；主题 0..6 与 type 5 封面流程一字不改。

## 建议实机验证

1. 侧载本 IPA（板上已是 musicA.76），播一首歌抓日志确认 `[CFQ] slots=3 ok=111 shift=0`。
2. 三张侧卡是明显不同的专辑封面；「封面背景」切四档，圆屏背景随之变化。
3. 重连蓝牙不切歌也能看到 3 张卡；切回主题 0..6 无回归。
