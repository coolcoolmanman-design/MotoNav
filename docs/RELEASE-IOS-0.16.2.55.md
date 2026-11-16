# iOS 0.16.2.55

配套固件 `7.12.10-ui.2-zelda40.9-musicA.57`。基线 0.16.2.54。

## 更新 32 张组合预览 PNG

`scripts/bake-combo-builtin.py` 改用显式 alpha 蒙版重烘焙，App 预览与圆屏同源：
车窗玻璃、黑车架、轮胎、深色描边与衣物不再被抠成透明。
`CenterSpriteBridge.rebuild()` 仍直接读 `UIImage(named:)`，业务代码无改动。

其余修复都在固件端：

- 矢量简图 id 13 改参照龙珠雷达画完整主导航路线（`drawRouteRange` 从当前位置
  到终点），不再画前方 200m 窗口折线，线连续贯穿屏幕；
- 组合精灵恢复 6 相 bob / 上半身挥手摆动 / 按车型分档尾气 / 速度线 /
  自行车轮转提示；8 个 sleep-wave 同源组合未连接时压暗以区分两态。

真实路线（bulk type 2）与 `#p` 定位一直照发，iOS `MapFeeder` / `Settings` /
`CrossImageSender` / `CenterSpriteBridge` 逻辑均不变。

## 建议实机验证

- 导航中切第 7 种「矢量简图」：主路线是否完整连贯、跟随实际路线转弯、
  不再在窗口边界断开。
- 待机页切 16 组合：深色图案元素是否齐全、挥手与车辆抖动是否恢复、
  连接 / 未连接两态是否可区分且不再随机滚动。
