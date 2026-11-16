# 固件 7.12.10-ui.2-zelda40.9-musicA.51

配套 iOS：`0.16.2.48`。基线 musicA.50（`ff9c53a`）。

## 一、第 7 种地图样式「矢量简图」(id 13)

- `MapState.h`：`THEME_N 13 → 14`，两套主题各加 id 13，`#define ST_VECTOR 13`。
  昼 `0xF7BF/#0A63FF`、夜 `0x0862/#19E3B1`，色值由 932×932 参考图主色统计反推。
- `NavigationPage.h` 新增 `drawVectorHeroFrame()`，`drawNavFrame()` 在 `mapModeActive()` 之前分流：
  - 外圈细环 r=225…227 + 进度弧 `fillRingBand(190/360, 322/360, 222, 228)`（0=正上方顺时针，约 37% 圈，参考图起止角）；
  - `vhBigArrow()` 左上 V 形箭头 + 直角下折尾，右转做 x=136 镜像，直行 / 到达换形；
  - 距离数字左对齐 x=195（size 8 字模）+ 单位（size 3）；
  - `vhRoute()` 白描边 19 + 主色 12 的圆角折线、`vhCarArrow()` 车头白三角 + 光晕；
  - 示意绿地 / 水面 / 两根灰主干道全部程序化，不读瓦片、不新增 bulk；
  - `vhLanePill()` 中部胶囊 (118,168,150,44) 复用 `drawLaneGlyph()` + `laneFlags()`，
    无 `#j` 数据时自动不画；
  - 底部限速牌 (160,386) / 大字车速 (205,350,size 7) / 时间 · 剩余里程 (233,424)。
- **无红绿灯倒计时胶囊**（无数据源）。
- 路口放大图与底部车道条在该样式下完全不画：`crossReady()` 与 `mapModeActive()`
  对 12 / 13 双双提前 return，且新帧在 `drawNavFrame()` 最前面分流，两者都不会被调用。
- 超速时进度弧与车速转 `#FF3B30`，并保留整圈超速红边。

## 二、iOS 配套

- `Settings.swift`：`themeOrder` 加 13、名称 / 描述、`crossHiddenThemes = [12,13]`。
- `MapFeeder.swift`：`THEME_NO_BASEMAP = [11,12,13]`，建筑块两处 `theme != 11` 收敛为集合判断。
- `CrossImageSender.swift`：`send()` 在该样式下直接返回，省掉量化与 BLE 传输。

## 三、协议 / 性能

- 零新增命令、零新增 bulk：沿用 `#t,13,1` / `#j` / `#v` / `#k` / `#e`；主题不存 NVS，由 App `pushAll()` 下发。
- 不下 1200² 栅格底图、不跑双核 `parallelRows`；每帧只画约 10 条折线 + 1 个圆角胶囊 + 3 组文字。
- 老固件收到 `#t,13,1` 因 `t >= THEME_N` 被静默忽略。
