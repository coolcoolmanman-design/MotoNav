# 固件 7.12.10-ui.2-zelda40.9-musicA.57

基线 musicA.56 / iOS 0.16.2.54（`404d7aa`）。本轮修 3 个实机反馈，
**协议 / bulk 类型 / iOS 业务代码零改动**（iOS 仅更新 32 张预览 PNG 与版本号）。

## 一、矢量简图 id 13：改画「完整主导航路线」（参照龙珠雷达）

实机反馈：56 的前方 200m / 16 点窗口折线「导航线不完整」。
根因是 ST_VECTOR 自己造了一套窗口投影，窗口边界一裁剪就断，且只覆盖前方一小段；
龙珠雷达（ST_DRAGON）之所以路线连贯，是因为它走 `drawMapFrame()` 的标准视图 +
`drawRouteRange(split → 终点)`，`mapPolyline` 自带视口裁剪，线连续贯穿屏幕。

- 删除 `vhBuildRealRoute()` 及 musicA.56 的 4 处窗口几何（16 点采样 / 200m 窗口 /
  矩形裁剪 / 航向平滑）与 `vhRoute()` / `vhCarArrow()`。
- 新增 `vhDrawFullRoute()`：复用 `predictPose()` + GPS 平滑修正 + `viewHdg` 时间平滑
  + `viewScale` 随车速，再调 `drawRouteRange(routeProg, routeN-1, …)` 画
  **从当前位置到终点的整条剩余主路线**（白描边 + 主色内芯），已走过的一段压暗；
  车用 `drawPlayer(CX, MAP_PY)`，与龙珠雷达同款在中心随车移动。
- `drawVectorHeroFrame()` 不再每帧 `viewInit = false`（否则朝向平滑被重置）；
  主循环对 ST_VECTOR 每帧置 `navDirty`，按龙珠雷达节奏连续刷新。
- HUD 用 `vhFadeRect()` 半淡化（82% 往背景色）而不是硬遮罩，路线在文字下方仍连续可见。
- 进度弧 37% 圈、超速红边、车道胶囊、路口图与车道条隐藏规则、昼夜配色全部不变。

## 二、16 组合：图案元素透明缺失（用户判断正确，就是抠图过度）

根因有两层，都在 `scripts/bake-combo-builtin.py`：
1. `fg = (L>=70)|(chroma>=40)` 用亮度定义前景 → 车窗玻璃、黑车架、轮胎、
   深色描边与衣物**根本没进最大连通 blob**；
2. `make()` 用 `valid = lum(r) >= 20` 当透明判据，LANCZOS 缩放后的暗过渡边
   和主体深色像素全被写成 `0x0000` 透明键，`to565()` 的 `0x0001` 不透明黑哨兵
   因此永远不触发。

修复：改为**显式 alpha 蒙版** —— 用宽松种子 `(L>=18)|(chroma>=14)` 找最大连通 blob，
`findContours(RETR_EXTERNAL)` + `drawContours(FILLED)` 填掉内部孔洞（车窗/轮胎/车架），
再 erode 收回膨胀；蒙版随图一起 LANCZOS 缩放，`valid = mask >= 80`。
主体内深色像素一律保留，纯黑用 `0x0001` 哨兵。App 预览 PNG 用同一蒙版生成，
与圆屏完全一致。32 帧已重烘焙，zlib 总 578,742 B（565 KiB），仍在 app 分区内。

## 三、16 组合：挥手 / 车辆抖动等动画恢复

根因：旧 CSP 通路 `CenterSpritePixelizer` 有 6 帧 sleep + 6 帧 chase 的**程序化动画**
（整帧 bob、Zzz、按车型分档尾气、自行车踏板轮转），musicA.55 改成 2 张静态 PNG
后全丢了；且 10/16 组合的 sleep/wave 源图**逐字节相同**，两态看起来完全没变化。

`HomePage.h::drawComboBuiltin()` 用零额外资源把动画搬回固件：
- 6 相 bob（sleep 0..3px / chase 0..4px，平滑且严格按真实 BLE 连接态，
  不再有 fix4 的 4 秒强制轮播与 9px 弹跳）；
- 源图上半部 45% 做水平摆动模拟挥手，下半部车轮保持稳定，不会整车乱晃；
- 尾气按车型分档（超跑/竞速摩托浓、踏板摩托淡）+ 车后速度线；
  自行车 / 折叠小轮改用双轮心旋转亮点（原 pedalHint）；Zzz 上浮保留；
- `COMBO_SAME_MASK 0xE8F0` 标记 8 个同源组合，未连接态整体压暗 72%，两态可区分。

## 四、兼容与验证

- 未擦 NVS；`#CS` / `#FC3` / `#MBG` / `#t` 等设置全部沿用；
  开机即有、选择即生效、零 BLE bulk 不变。
- 全部源码保持 CRLF，`git -c core.whitespace=cr-at-eol diff --check` 通过。
- 待实机：矢量简图路线完整连贯 / 组合图案深色元素齐全 / 动画恢复。
