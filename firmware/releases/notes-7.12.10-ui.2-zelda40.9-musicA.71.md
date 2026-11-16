# 固件 7.12.10-ui.2-zelda40.9-musicA.71

基线 musicA.70 / iOS 0.16.2.64。**固件 + iOS 双端同包**（FO-217 + FO-218），配套 IPA **0.16.2.65**。
安卓端本版不改。两份 release 保持 draft，不 publish。

## FO-217 · 动态背景新增「像素夜景」（style 6）

按确认稿 `pixel-bg-mockup.html` 的**方案 A 深色夜景版**落地，沿用 musicA.40 起「动态背景恒深色」的既有约定，槽位文字颜色零改动。

- `graphics/DynamicBg.h` 新增 `dynBgRenderPixelCity()`：78×78 调色板网格 × 6px 放大到 466²，复用现有的 Q15 定点、正弦表与 `dynBgBuf`（PSRAM 424 KiB）→ `bgSrc()` → `zbgRect()` 链路。
- 全部动画由相位程序化生成，零 BLE 传输、零文件存储：路面虚线 8 相位滚动、骑手 2 帧 bob、轮辐 4 相位、星窗明灭、月牙与霓虹楼群。LCG 每帧重置固定 seed，保证楼群与星点位置不跳。
- `dynBgRenderFrame()` 在 `style==6` 提前 return；`#BG,<on>,<style>,<speed>,<auto>` 的 style 上限由 `0..5` 放宽到 `0..6`，NVS `bg_*` 与 `dynBgActive()` 门控（自定义壁纸 / 省电自动让位）保持原样。
- 体积：新增代码约 221 行，无帧级位图资源；编译后 Flash 2,938,039 B（93%），余量约 203 KiB。

## FO-218 · 音乐页新增「黑胶唱片」（style 6）

按参考图 `downloads/audiopod-ref.png` 与效果图 `vinyl-music-compare.png` 复刻，挂在已有 6 种音乐样式之后。

- `pages/MusicThemes.h` 新增 `ms6Base()`（红盘径向渐变 + 斜向高光、黑胶同心沟槽 + 点积高光）、`ms6Label()`（按 `coverAngle` 反向旋转采样 `coverImg`）、`ms6Arm()`（复用 `tonearmT`，播放落槽 / 暂停抬离）、`ms6Text()`、`drawMusicThemeVinyl()`。
- `PageNavigation.h` 补分发、唱臂缓动与旋帧条件；style 6 旋帧降到 **10fps**，避免每帧重画 16 万像素。
- `#MS,<0..6>` 放宽上限，NVS `mstyle` 不变；触摸命中：点中心封面播放/暂停，左右两侧切歌（参考图无可见按键，不另画播控）。
- 布局：黑胶中心 (233,158) r152 / 中心封 r56 / 红盘中心 (233,258) r178 / 唱臂圆钮 (392,236)；歌名 y340、歌手 362、时间胶囊 385、状态小字 409。参考图的 `1/2` 分页标号改为真实播放时间 `m:ss`（固件无队列序号）。
- 旋帧与唱臂动画只在 `musicPlaying` 时推进；暂停时唱臂抬回盘外。

## 配套与刷新

- **必须同升** FW musicA.71 + IPA 0.16.2.65 才能在 App 里选到两个新样式；固件单独升级不会自动切换 style。
- 两个新样式都是**追加**，不改变 0..5 旧样式的默认行为；NVS 里存的是旧值的用户仍落在原样式。
- 不动 Infograph BASE / type8 底图、不动 include 顺序、不动安卓。

## 已知边界

- 像素夜景在 `centerFunc==2`（全屏表盘 / C Infograph）下不生效：`animateCat()` 会短路且全屏表盘自绘底图。
- 自定义壁纸优先于动态背景（`bgCustomOn` 时 `dynBgActive()` 自动为假）；省电模式同样让位。
- 10fps 是整屏推屏上限，未做局部 466×224 刷新优化。

## 建议实机验证

1. 设置 → 动态背景选「像素夜景」：星月、霓虹楼群、路面虚线滚动、骑手 bob 是否流畅；切回旧 5 种样式是否正常。
2. 设置 → 音乐样式选「黑胶唱片」：中心封面慢旋、唱臂播放落槽 / 暂停抬起、红盘层次与四层文字是否清晰。
3. 全屏表盘模式下确认不串出像素背景；自定义壁纸上传后像素夜景自动让位。
