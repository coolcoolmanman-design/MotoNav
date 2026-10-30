# 通用圆屏固件 7.12.11-opt.1

基于 TPMS 配套时代固件树（`ios-v0.15.12-tpms` 源码包内 `boards/175` = 7.12.9-junction.11）与已合入的封面 bulk type 5（7.12.10-cover.1），仅合入 PR2 / PR5 固件侧。

## 本版改动

### PR2 — 地图 Deflate + XOR Delta
- 能力：鉴权后主动通知 `CAP,MD,1`；手机可发 `#MD?` 查询。
- 解码：bulk type 3 在既有 RLE 之外识别 `DFL`（二次 zlib）与 `DXR`（位移 + XOR 平面 zlib）。
- 兼容：无魔法头的旧 RLE 包路径不变；旧手机无需升级。

### PR5 — 极简音乐页
- 去掉 `PAUSED` / `PLAYING` 与 `SWIPE LEFT TO RETURN`。
- `#Z,have,play,mask[,dur_ms,elapsed_ms]` 可选时长；本地推演进度条。
- AMS / HID 仍用 Apple Media 时长；封面 bulk type 5（`CVR`）布局保留。

## 跳过
PR1 / PR3 / PR4 / PR6；手机侧 Deflate/XOR 编码与 `#Z` 时长下发待后续 iOS/Android 同步。
