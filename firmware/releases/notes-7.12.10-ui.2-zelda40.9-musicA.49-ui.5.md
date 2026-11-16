# MotoNav 通用固件 7.12.10-ui.2-zelda40.9-musicA.49-ui.5

基于 ui.4 的导航结束状态修复，配套 iOS 0.16.2.45。App 资产：`0x10000`（不擦 NVS）。

## 修复

1. `CommandReceiver.h::resetNavState()` 原来只清 `inNav`，残留的 `navDir="A" / navDist=0 /
   tripRemainM` 会在任何一次 `navDirty` 重绘时把上一趟「剩余 0」的到达帧画回导航页。
   现在一并清掉 `navDir / navDist / navAngle / tripRemainM / tripEta / tripLights / navDirty`，
   并补一次 `progReset()`（`navDir` 清空后新导航首帧不再触发进度重置，在这里补上，
   避免进度条延续上一趟）。
2. 拦截「孤立到达帧」：清屏 / 断连后收到 `A,0` 且 `inNav=false`、`prevDir` 为空时直接丢弃，
   不再置 `inNav`。真正到达时 `prevDir` 必是 `L/R/S/...` 方向，导航途中圆屏重启补发的
   是方向帧（`newDir != "A"`），两种情况都不受影响。**这条是固件侧独立兜底**：
   即使配旧 App（仍会重连补发 `A,0`）也不会复现。
3. `MotoNav.ino` 到达特效结束（点 OK，或 60s 兜底自动关）后，若 `navDir == "A"` 就清
   `inNav` 并回待机页，不再回画导航帧；途经点特效（`navDir` 不是 `A`）仍照常回导航页。

## 不变

- 任何 BLE 命令格式、`#` 指令、bulk 协议、图片尺寸都没动；`C` 清屏语义不变。
- 卫星图空白修复、GTA 下半动画、路口放大图、动态背景、六槽 / 时间样式等 ui.4 功能不动。
- 断连自动清状态、5 分钟自动关机、到达庆祝与设备端 OK 等既有逻辑不变。

## 建议

与 iOS 0.16.2.45 一起升级。只刷固件也能挡住旧 App 的重连补发帧（第 2 条），
但 App 侧清缓存才能同时解决 `C` 之后的队列尾帧。
