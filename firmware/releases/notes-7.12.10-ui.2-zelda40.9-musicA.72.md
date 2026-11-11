# 固件 7.12.10-ui.2-zelda40.9-musicA.72

基线 musicA.71 / iOS 0.16.2.65。**固件 + iOS 双端同包**（FO-219 实机反馈修正），配套 IPA **0.16.2.66**。
安卓端本版不改。两份 release 保持 draft，不 publish。

## FO-219 · 黑胶唱片音乐页两处实机修正

### ① 歌名 / 歌手 / 时间胶囊不再重叠

`drawMusicMetaCentered()` 走的是 `drawTextImgC(slot, y0, ...)`，而 `y0` 是**文字位图的顶部坐标**，不是中心。
App 下发的歌名位图约 39px 高（32pt）、歌手约 29px（24pt），且 `drawTextImgC` 会按圆内半径 `TEXT_R=200` 等比缩小；
旧值 340 / 362 / 385 / 409 在实机上算得：

| 相邻层 | 旧稿 | 新稿 |
|---|---|---|
| 歌名 → 歌手 | **重叠 7.6px** | 间隙 7.7px |
| 歌手 → 时间胶囊 | **重叠 9.8px** | 间隙 13.4px |
| 胶囊 → 底部小字 | 间隙 11px | 间隙 6px |

新常量：歌名 y316 / 歌手 y358 / 胶囊中心 y405 / 底部小字 y424；胶囊内时间文字由 `CAP_Y-1` 改为 `CAP_Y-8`（size2 字高 16，正好居中于 26px 高的胶囊）。短歌名不触发缩放（s=1，高 39）时仍有 3px / 5px 余量，不会重叠。
仿真脚本 `sim-vinyl-layout.py` 逐句复刻了固件缩放规则，产物 `vinyl-layout-compare.png` / `vinyl-layout-detail.png`。

### ② 唱片旋转提速、不再卡顿

旧实现：旋帧每 100ms 全量重画（约 10fps），且 `ms6Base()` 对整屏 16 万像素做两遍 `sqrtf` 渐变，
`ms6Label()` 再反向旋转采样封面，单帧开销远超 100ms；角速度 `2π/36000` 是 **36 秒才转一圈**。
此外每 500ms 进度刷新、每 1000ms AMS 刷新、唱臂缓动都会置 `musicDirty` 触发全量重画，造成周期性掉帧。

新实现：

- `MusicThemes.h` 新增三块互不重叠的 PSRAM 静态层快照（中心封 119×119 / 唱臂 96×80 / 文字带 366×124，合计约 126 KiB），
  在 `ms6Base()` 之后、`ms6Label()/ms6Arm()/ms6Text()` 之前捕获；`drawMusicThemeVinylSpin()` 每帧只恢复中心封区并重画（约 1.4 万像素），
  唱臂与文字带仅在 `ms6ArmDirty` / `ms6TextDirty` 时局部重画。
- 周期性 `musicDirty`（500ms 进度、1000ms AMS）对 style 6 改为 `ms6TextDirty`；唱臂缓动改为 `ms6ArmDirty`，都不再触发全量。
- 帧率统一到 **24fps**（`dt>=42ms`），角速度提到 **12 秒/圈**（`2π/12000`）。
- PSRAM 分配失败时自动退回全量路径，不会黑屏。

编译：Flash 2,938,887 B（93%）/ RAM 144,796 B（44%），三块缓存是运行时 `ps_malloc`，不占静态 RAM。

## 动态背景：新增门控诊断日志（功能未改）

收到用户反馈「选了动态背景只有黑色背景」。核查后确认 `dynBgActive()` 门控与页面链路本身没有改动：

- `bgSrc()` 优先级：`bgCustomOn` 自定义壁纸 > `dynBgActive() && dynBgBuf` 当前帧 > `homeBg`（恒 nullptr = 透明黑）。
- 因此 `dynBgOn=false` / `bgCustomOn=true` / `lpModeOn=true` / `dynBgBuf` 分配失败都会让 `zbgRect()` 走 `fillRect(C_BLACK)`，看到纯黑。
- 动态背景**只在待机首页 `animateCat()` 生效**：音乐页、导航页、C Infograph 全屏表盘等自绘底色，不读 `dynBgBuf`。

为便于实机一锤定音，`#BG` 处理现在**每次收到命令都无条件打印**：
`[BG] on=.. style=.. speed=.. auto=.. active=.. custom=.. lp=.. center=.. buf=.. page=.. wait=..`
（旧版只在值变化时才打印，导致开机日志看不到状态）。
App 设置页同步补一行说明：仅待机首页（中央=动画、非全屏表盘）生效。

## 建议实机验证

1. 圆屏停在**待机首页**（中央=动画），App 选「像素夜景」，确认星月 / 楼群 / 虚线滚动 / 骑手 bob；抓开机日志看 `[BG] active=1 custom=0 lp=0 buf=1`。
2. 音乐样式选「黑胶唱片」：歌名、歌手、时间胶囊、底部小字四层不重叠；中心封面约 12 秒/圈、24fps 顺滑；播放/暂停时唱臂落/抬无全量卡顿。
3. 切到全屏表盘 / 音乐页时动态背景不显示属预期（页面自绘底色）。
