# 固件 7.12.10-ui.2-zelda40.9-musicA.56

基线 musicA.55 / iOS 0.16.2.53（`5fd9946`）。本版只修「矢量简图 id 13」中部折线的
4 处几何缺陷，**协议 / bulk 类型 / iOS 代码零改动**。

## 复核结论：数据链本身是通的

musicA.55 已经把 `routeX/routeY` 接上，本轮从头复核确认：
`MapFeeder.onRoute` 不受 `THEME_NO_BASEMAP` 门控、bulk type 2 一直在发；
`#p` 每次定位都会置 `navDirty`（`CommandReceiver.h:629`），所以虽然
`mapModeActive()` 在 id 12/13 下提前 return false、主循环不再每帧强制刷新，
页面仍随每次定位重画；`routeFrame == posFrame`、`vhCumAt/vhUAtCum` 的
「段号+比例 ↔ 累积米数」换算、以及旋转公式都与 `MapFrame.h:72-73` 一致。
剩下的乱线全部来自 55 新写的投影几何。

## 4 处修复（`NavigationPage.h::vhBuildRealRoute()`）

1. **贴边 clamp → 矩形裁剪出画**：原来 `constrain(sx,34,432)` /
   `constrain(sy,206,306)` 把越界点钉死在窗口边界，后续点连成贴边水平/垂直长线。
   现改为与上一个点做参数裁剪，把交点作为折线终点后 `break`，折线自然出画。
2. **车吸附路线、固定在 (233,300)**：投影原点从「推算车位 `px,py`」改为
   「车在路线上的投影点 `routePt(routeProg)`」，折线起点恒等于车位；
   车头箭头由 `vhCarArrow(p[0],p[1],…)` 改为固定 `VH_CAR_X/VH_CAR_Y`，偏航不再乱跑。
3. **缩放恒定**：删掉 `L = constrain(navDist+80,120,500)`，改固定
   `VH_WIN_M = 200m`、`sc = 94/200`。原来每次 `#n` 都改 `sc`，同一段路每帧
   「呼吸」，接近终点 `cumEnd` 被截断时还会突然放大。
4. **16 点采样 + 航向时间平滑**：原来等弧长只抽 6 点，直角弯被抹平/错连；
   现 `VH_SAMPLES 16`（约 13m 一点）保留弯道。航向不再直接用 `posHdg`，
   改与 `MapFrame.h` 同款按时间收敛（0.22s 常数、每帧最多 70°·dt / 12°、
   掉头 >90° 直接对齐），低速 GPS bearing 抖动不再带整条线左右摇摆。

## 兼容与验证

- 无真实路线（未导航 / 坐标系错帧 / 偏航 >50m / 已到终点）时仍降级到原来的
  示意折线 + 假绿地 / 水面装饰；真实路线可用时依旧不叠这些装饰。
- `p[12]` → `p[32]`，`vhRoute()` / `vhCarArrow()` 签名不变；进度弧固定 37% 圈、
  超速红边、车道胶囊、路口图与车道条隐藏规则全部不变。
- 组合精灵 16 组合固件内置、`#CS` 索引、时钟重设计等全部保留，无回退。
- 未擦 NVS；全部源码保持 CRLF，`git -c core.whitespace=cr-at-eol diff --check` 通过。
