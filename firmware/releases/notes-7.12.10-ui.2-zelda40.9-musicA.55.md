# 固件 7.12.10-ui.2-zelda40.9-musicA.55

基线 musicA.54 / iOS 0.16.2.52（`9f35a5d`）。本版解决用户实机反馈的 6 个问题，
**协议新增 `#CS,<g>,<v>` 一条命令，其余协议 / bulk 类型零改动**。

## 一、组合精灵：16 组合烧进固件，选择即生效

过去是「App 运行时像素化 → BLE bulk type 11（638 个 ATT 分片 / 100–310 KB）→ ffat 落盘」，
所以切换不即时、还要等推送。现改为固件内置资产：

- 新增 `scripts/bake-combo-builtin.py`：从 32 张 466×466 PNG 重烘焙
  （抠掉 `(12,14,18)` 深蓝黑面板、暗圆环、残留 `W/E` 与「骑行」文字 → 保留最大连通主体；
  sleep/wave 用同一并集 bbox 裁切；汽车 236×150 / 两轮车 210×186 目标框）。
  原始素材另存 `scripts/combo-src/`，脚本幂等（重跑输出字节一致）。
- 新增 `resources/ComboBuiltin.h`：`PROGMEM` 32 帧（16 组合 × 2 态），
  每帧 256×192 RGB565 LE + 色键 `0x0000`，zlib level 9；**总计 416,810 B = 407 KiB**
  （app 分区 3 MiB 已用 2.34 MiB，余 783 KiB，烧完还剩约 330 KiB）。
- `HomePage.h::drawComboBuiltin()`：从索引表取 zlib 帧 → PSRAM → 最近邻放大 1.25×
  到 320×240，锚点 `(233,248)` 与 GTA 一致；逐像素色键跳过。
- **撤销 fix4 的 `((now/4000)&1)` 4 秒强制两态轮播**（与真实连接态无关，就是实机
  「两个动画随机滚动」的主因），改回严格按 BLE 连接态切 sleep/chase。
  CSP 回退通路帧周期 3500→3600（能被 6 整除）、相位改进入该态后的相对计时，
  消除状态边界帧号乱跳。
- 动画改为**局部装饰**（sleep 上浮 Zzz / chase 扩散尾气），不再整帧 bob 平移；
  `lpModeOn` 省电冻帧语义不变。
- 新增 `#CS,<g 0..1>,<v 0..7>`（NVS `cs_gen` / `cs_veh` / `cs_bi`），回执 `CS,g,v`；
  `#CS,-1,-1` 可切回旧 CSP 自定义通路。老固件忽略未知命令，双向兼容。
- App 预览与固件同源：重烘焙后的 32 张 PNG 直接写回 `Assets.xcassets`，
  iOS 不再做运行时像素化 / CG 翻 Y —— 预览与圆屏同时正立。

## 二、矢量简图 id 13：导航线改读真实路线

**根因**：`drawVectorHeroFrame()` 过去完全没读 `routeX/routeY`，只有 3 条写死折线
按 `navDir` 三选一 + 左右镜像；`vhProgress()` 又把「当前段剩余距离」`navDist` 当全局
进度，过路口重置 → 车头沿折线**倒着飞回起点**；ST_VECTOR 在 `drawNavFrame()` 最前面
提前 return，绕开了 `drawMapFrame()` 的 `predictPose` / `updateRouteProgress` /
`drawRouteRange`，所以已有的真实路线数据链一次都没跑。

- 新增 `vhBuildRealRoute()`：`predictPose()` + `updateRouteProgress()` 刷新 `routeProg`，
  用 `routeCum` / `routePt()` 按弧长把「前方 `max(120, navDist+80)` 米」等距抽稀成 6 点，
  按 `posHdg` 旋转（**车恒在窗口起点 `(233,300)`、车头朝上**）投到简图区 y=206..306。
  数据来源是 App 一直在发的 bulk type 2，`THEME_NO_BASEMAP=[11,12,13]` 只挡底图不挡路线。
- 车头固定在折线起点朝上，不再随 `navDist` 反推；连续路口 `navDir` 变化不再整帧镜像。
- 真实路线可用时**不画假绿地 / 假水面 / 两根假灰路**（过去与真实折线叠在一起就是
  「导航线混乱」）；无路线数据时才降级到从车头向上的示意折线。
- 进度弧固定参考图 37% 圈，不再随 `navDist` 跳变；超速红边保留。
- `vhPointAt()` / `vhProgress()` 及其静态 `vhProg` 状态整体移除（过去从不复位，
  切主题会继承上一段进度）。
- **iOS / 协议 / bulk 类型 / 底图门控 / 路口图与车道门控全部零改动**。

## 三、兼容与验证

- 设备当前刷的 musicA.54 及其后所有版本（组合精灵、电量管理、id 12 大字转向、
  id 13 矢量简图、时钟重设计）全部保留，无回退。
- 未擦 NVS；已有 `#FV` / `#FC3` / `#MBG` / `#t` / `#CS` 设置沿用。
- 全部源码保持 CRLF，`git -c core.whitespace=cr-at-eol diff --check` 通过。
