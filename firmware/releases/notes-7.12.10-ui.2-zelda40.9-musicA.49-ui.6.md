# 固件 7.12.10-ui.2-zelda40.9-musicA.49-ui.6

配套 iOS：`0.16.2.46`。基于 ui.5（`f5550ca`）继续迭代，协议对旧 App 保持兼容。

## 一、地图新增第 6 种样式「大字转向」（A 方案）

参考图确认稿落地。新增 `ST_BIGTURN 12`，`THEME_N 12 → 13`，`THEMES` / `THEMES_NIGHT` 各补一行。

- `NavigationPage.h` 新增 `drawBigTurnFrame()`，在 `drawNavFrame()` 入口**早于 `mapModeActive()`** 分流：
  - 纵向渐变底（昼 `#F4F9FF→#C9E0FF`、夜 `#12243A→#050B14`），复用 `blend565()`；
  - `drawBigIconAt()` 放大转向箭头（可用区 68~218、maxW 210、laneK 1.35）；
  - `printCentered(..., 8, ink)` 超大距离数字 + `setTextSize(2)` 小单位；
  - `drawTextImgC(0, 336, 0xE8, ink)` 路名按昼夜着色，车速小字；
  - `fillRingBand()` 淡底圈 + 参考图式 0.79 弧，转弯倒计时期间切换为 `drawRingProgress(ringShown, 5, acc)`；
  - 夜间主色严格用琥珀黄 `0xFDA0`（`#FFB703`），白天 `0x135F`（`#1668FF`）。
- **路口放大图与车道自动隐藏**：
  - `CrossImage.h::crossReady()` 开头对 `ST_BIGTURN` 返回 false。这是硬约束——该函数会在 `mapModeActive()` 里强制返回 true，不屏蔽会把大字页挤回地图页。一处生效连带关闭浮窗、车速改位、车道让位、避障占位框。
  - `MapPrimitives.h::mapModeActive()` 开头同样返回 false，避免主循环按地图页每帧置 `navDirty`。
  - 新样式走独立绘制函数并 `return`，不调用 `drawLanes()` / `drawMapFrame()`，车道自然不出现，无需改动 `LaneDrawing.h`。
- **保留超速红边**（`hasCam && camLimit > 0 && speedKmh > camLimit` → `fillRingBand(0,1,228,233,C_RED)`，车速小字同步变红）与**电量**（直接调 `drawBattery()` + 蓝牙点；不用 `drawStatus()` 是因为它带一条黑底擦除带，会破坏浅色背景）。
- 协议零改动：`#t,12,1` 复用现有主题命令，第三字段仍会清掉旧栅格/矢量缓冲；老固件 `t < THEME_N(12)` 不成立即静默忽略。iOS `MapFeeder` 把 11/12 一并列入免底图集合。

## 二、音乐封面风格化背景（固件端自算）

新增 `src/pages/MusicBg.h`（插在 `BadgePage.h` 之后、`PageNavigation.h` 之前，避开单编译单元声明顺序坑）。

- 档位（本版定为 3 档 + 关闭，强度固定标准档）：
  | mode | 处理 | 网格 | 放大 |
  |---|---|---|---|
  | 0 | 关闭，保持原 `coverAmbient` 纯色 | — | — |
  | 1 | 放大虚化（默认） | 32×32 区域均值 | 双线性插值 |
  | 2 | 色块马赛克 | 10×10 区域均值 | 硬边放大 |
  | 3 | 双色海报 | 64×64 亮度 4 档映射 | 硬边放大 |
- 公共处理：中心 1.55× 裁切采样 → 3×3 邻域均值（天然模糊，保留大尺度色块）→ 压暗 40% / 去饱和 15% → 径向暗角 34% → 圆外清零。
- **只在换封面或换档位算一次**：`parseCover()` 与 `#MBG` 置 `musicBgDirty`，`musicBgBlit()` 惰性调用 `musicBgBuild()`；之后每帧仅 `memcpy` 434,312 B 到画布（原逐像素刷纯色反而更慢）。
- 缓存 `musicBgBuf` 为 PSRAM 466×466 RGB565（434 KB），懒分配；**分配失败置 `musicBgFailed` 并永久退回纯色**，不崩溃。网格 `mbgGrid[64×64]` 仅 8 KB 内部 RAM。
- `PageNavigation.h::drawMusicAmbient()` 改为优先 `musicBgBlit()`，失败/关闭时走原纯色分支（零回归）。
- **样式 4 极简仪表**：删掉 `MusicThemes.h` 里硬编码 `0x1082` 的整段深底循环，改为 `drawMusicAmbient()`，5 种播放样式统一吃风格化背景；`#MS,0..4` 与 5 种样式数量完全不变。
- 新命令 `#MBG,<0..3>`（早于 `CommandReceiver.h` 的未知 `#` 忽略分支），NVS 键 `mbg`，默认 1；回执复用 `pendingVehAck` 通道，格式 `MBG,<mode>`（`-1` = 保存失败）。
- 音乐页仍不读 `nightOn`、不走 `bgSrc()/zbgRect()`，与昼夜配色 / 自定义壁纸 / 待机动态背景互不干扰。

## 三、兼容性与升级

- 协议只新增一条可选短命令 `#MBG`，其余全部复用；旧固件忽略未知 `#` 命令，旧 App 不显示新选项，新旧双向兼容。
- 固件主题不存 NVS，靠 App `pushAll()` 下发；新样式与背景档位在连接/重连时自动同步。
- 刷机用 `..._app_0x10000.bin`，不擦 NVS。

## 四、待实机验证

1. 大字转向昼夜两套色的实际可读性、不同路名长度下的排版、导航进出与到达特效让位。
2. 封面背景 4 档在真实专辑封面上的观感与换封面时的一次性计算耗时（预计 20~40ms）。
3. 样式 0 旋转播放 42ms 帧预算、PSRAM 峰值（新增 434 KB）、温升与待机电流。
