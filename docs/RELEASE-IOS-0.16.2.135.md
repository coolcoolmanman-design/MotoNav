# iOS 0.16.2.135 — 样式 16 路名纯文字化 + 锁屏地图回退（配 musicA.150）

基线 0.16.2.134。只改 iOS 侧两处，**不动固件协议**。

## 1. 样式 16 的路名：只发纯路名，字号放大

参考稿（昼夜通透样式的设计稿）路名只有一个大字路名，没有「进入 / 沿…直行」前后缀。
原来所有样式共用 `updateTexts()` 的拼串与 26pt 字号，实测在 466 圆屏上偏小、且多出来的字
被固件截断得很尴尬。

- `updateTexts()` 新增 `glassTheme = (Settings.shared.theme == 16)`；
- 样式 16：`road = nextRoad 非空 ? nextRoad : curRoad`，`TextImage.send(slot: 0, road, maxW: 300, size: 36, minSize: 26)`；
- 其它样式完全不变（26pt / minSize 18 / 前后缀照旧）。

固件侧 `drawTextImgGlass()` 现在按 HUD 竖列中心（x≈238）居中、中心 y370，压在底部胶囊上沿之上，
白字带右下软投影（真 alpha 混合）。

## 2. 锁屏 / 后台：高德源回退天地图底图（修「瓦片显示一会儿后消失」）

样式 15（矢量 3D 鸟瞰）/ 16（昼夜通透）的底图来自**高德 SDK 截图**，而截图必须前台渲染。
一锁屏 `setRenderingAvailable(false)` 就在途取消，车再开出原图块后 App 补不上新块，
圆屏那边看起来就是「地图显示一会儿后整块消失」。

- 新增 `lockscreenHttpFallback`：`mapSource == 高德 && !appActive && !satMode && theme ∈ {15,16} && 天地图 Key 非空`；
- 并入 `httpBasemap` → `canCapture` 为真，锁屏也能取底图；`fetchChunk` 的 worker 走进天地图
  `vec_w` 分支（`else if src == SRC_TDT || tdtFallback`），**底图包按 `SRC_TDT` 存缓存**，
  不污染高德缓存；
- 回前台 `setRenderingAvailable(true)` 照旧触发 `tryUpgradeBuildings()` / `fetchChunk`，
  恢复高德底图 + 建筑轮廓（楼块）；
- 失败重试退避沿用 HTTP 档（6s）；
- `buildingInfo` 提示分三种：锁屏回退天地图（无楼块）/ 锁屏只用缓存 / 锁屏无 Key 取不到高德底图
  （提示配置天地图 Key 或把地图来源切到天地图）。

有天地图 Key 的用户：锁屏期间地图继续刷新（无楼块）；没 Key 的用户至少沿线缓存可用，
不会整块变空 —— 固件那边同时把样式 16 的 sticky 放宽到 6 小时并做边长补边渐隐。

## 兼容

| App | 固件 | 结果 |
|---|---|---|
| 0.16.2.135 | musicA.150 | 完整（路名字号/位置与参考稿一致，锁屏不断图） |
| 0.16.2.135 | musicA.149 及更早 | 路名仍按旧版固件坐标（偏小、无玻璃软投影）；锁屏回退仍有效 |
| 0.16.2.134 及更早 | musicA.150 | 路名仍是「进入 XX路」26pt；锁屏仍可能断图 |

## 验证

- `NaviBridge.swift`：只改 `updateTexts()`，`pushNav` / `turnAngle` / 红绿灯 / 测速逻辑未动；
- `MapFeeder.swift`：新增两个计算属性 + worker 分支条件 + 三处缓存 source 变量 + 提示文案，
  其余抓图/编码/传输路径未动；
- 版本号：`RELEASE_VERSION=0.16.2.135`、`MARKETING_VERSION=0.16.2.135`、`CURRENT_PROJECT_VERSION=141`。
