# iOS 0.16.2.65

配套固件 `7.12.10-ui.2-zelda40.9-musicA.71`。基线 0.16.2.64 / musicA.70。
同包落地 **FO-217（像素夜景动态背景）** 与 **FO-218（黑胶唱片音乐页）** 两个新样式选项。安卓端本版不改。
两份 release 保持 draft，不 publish。

## 变更

### FO-217 · 动态背景第 7 项
- `Settings.swift` 的 `dynBgNames` 追加第 7 项纯文字「像素夜景」（遵守「新下拉框一律纯文字、不放缩略图」约定）；`musicA.37` 起的下发协议与 NVS 键不变。
- `DeviceHomeViews.swift`：设置说明里「6 种风格」改为「7 种风格」；`DynamicBgDial.styleOverlay` 加 `case 6` 的像素夜景示意，预览仍复用壁纸卡右上的 `backgroundPreview`（84px 圆 + 92px 描边）。
- 用户选择后由现有 `#BG,<on>,<style>,<speed>,<auto>` 下发 style=6；固件侧上限已同步放宽到 0..6。

### FO-218 · 音乐样式第 7 项
- `Settings.swift` 的 `musicStyleNames` 追加「黑胶唱片」，并把 `musicStyle` 的 clamp 上限由 5 放宽到 6。
- 仍走既有 `#MS,<id>` 下发与回执；封面与播放状态沿用原有 BLE 通道，无新协议。

## 配合固件

- FW `DynamicBg.h` 新增 `dynBgRenderPixelCity()`（style 6）；`MusicThemes.h` 新增 `drawMusicThemeVinyl()`（style 6），`PageNavigation.h` 补旋帧 / 唱臂缓动。
- **必须同升 FW musicA.71**，否则 App 里能选到新项但设备侧 `constrain()` 会把 6 截成 5。

## 已知边界

- 像素夜景在全屏表盘 / C Infograph（`centerFunc==2`）下不生效；自定义壁纸与省电模式优先。
- 黑胶唱片页参考图的 `1/2` 分页胶囊改为真实播放时间 `m:ss`（固件无队列序号）；无可见播控键，点中心封面播放/暂停、左右切歌。

## 建议实机验证

1. 侧载本 IPA + 刷 FW musicA.71 后，两个下拉框第 7 项均为纯文字且可选中、NVS 能记住选择。
2. 动态背景预览圆里能看到像素夜景示意；选中后设备端星月 / 楼群 / 虚线滚动与骑手动画正常。
3. 黑胶唱片页中心封面慢旋、唱臂落/抬、文字层次清晰；切回旧 6 种样式无回归。
