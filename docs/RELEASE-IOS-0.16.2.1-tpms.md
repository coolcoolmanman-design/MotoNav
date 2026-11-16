# MotoNav iOS 0.16.2.1-tpms

Draft-only · 配对固件 `7.12.10-ui.2-zelda40.8-mapfix.1`（Draft `untagged-4380ec993dbb636e3e74`）。

版本号沿上游 `0.16.2` Build16 线（本 fork 补丁 `.1`）；**不**采用上游 0.16.2 Connect UI。

## 地图空白 / 轮廓缺失 / 错乱网格（主修复）

相对本侧 `0.15.40`，从上游 `motonav-2026.11.01-r2`（`e122516` / phone.8 + Build16）**整文件移植**地图投喂管线：

| 文件 | 动作 |
|------|------|
| `MapFeeder.swift` | 上游替换：handoff / `compactBase` / roof yield / plain RLE；去掉本侧 MD/DFL/DXR + empty-mask bypass |
| `RasterProcessing.swift` | 上游替换：`hasMapContent` / `MapDeliveryPolicy` / `MapStyleTransfer`；删除 `RasterMd` |
| `MapGeo.swift` | 上游替换：白路 `l≥254` + `recoverGrayRoads`（去 mid-gray+despeckle 网格） |
| `RouteMapPlan.swift` | 上游替换：`MapHandoffBuffer` / `mayCaptureBuildings` / covering |
| `AmapSdkRaster.swift` | 上游替换：`preparedBase` + 空白 roof 画布 |
| `BleBulkWriteQueue.swift` | 上游替换：`prioritize` / receipt / wire meter；`cancel(types:)` 仅 1+3 |
| `BleLink.swift` | 上游地图基座 + 回贴吧唧 type7 / `#OFF` / Cover5 / MotionG / Connect battery 别名 |
| `RouteOfflineMaps.swift` | 上游（含 `suspendCapture`） |
| `PhoneClockSync.swift` / `MapSampleReport.swift` / `MapReportExport.swift` | 新增（上游依赖） |

## 液态玻璃底栏（同包）

- iOS **26+**：`GlassEffectContainer` + `.glassEffect(.regular.tint(.blue).interactive(), in: Capsule())` + `.glassEffectID("liquid-selection")` + spring `0.48/0.72/0.18`
- 三增强：行程轴 `GeometryEffect` 拉伸；容器内边缘融合；蓝色高光流动（非霓虹）
- iOS **15…25** 回退：既有 `LiquidGlassDroplet` + matchedGeometry + 拉伸
- Tabs：导航 / 轨迹 / 圆屏 / 我的。**不改**连接动画。

## 刻意保留（fork）

- 电子吧唧 bulk **type 7** / `CAP,BDG`
- TPMS、非机动车文案、液态玻璃壳、磨砂连接（本轮不改连接）
- **不**采用 0.16.2 Connect*/ContentView UI

## 根因（相对 0.15.40 不完整 cherry-pick）

先前只挑了息屏/`MotorReroutePolicy`/固件分步换图，**未**换掉本侧 MD/DXR + mid-gray 分类 + 硬编码 type3 插队；空白砖、轮廓饿死、网格噪声仍在投喂端。本包以 Build16 投喂模型对齐。
