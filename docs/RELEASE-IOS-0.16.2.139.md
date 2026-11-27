# iOS 0.16.2.139

> 对应固件：musicA.165 / musicA.166（P3D 串口诊断已在 .165）
> Base：0.16.2.138（`cursor/ios-138`）

## 修：原生 3D 底图「打包失败」

`.138` 里点「立即传一帧」只显示 `3D 底图：3D 底图打包失败`。原因是**尺寸超协议上限**，不是取图失败：

- `takeSnapshot` 回来的是 448pt × 3x = **1344×1344 px**；
- `Persp3DPacket.encode` 和固件 `Persp3D.h` 都硬限单边 **1024**（`P3D_MAX_W/H`），超了 encode 直接返回 nil；
- 于是每一帧都在最后一刻被丢掉，日志里连 `persp-captured` 都没有。

`AmapCaptureHost.decodePersp` 现在会先按长边缩到 **640px**（`perspMaxSide`）再量化。640 已经比屏幕 466px 宽，横向还有一点过采样；LUT 的 `v` / `mm` 按实际缩放比例同步修正（原来就按 `decoded.width / bounds.width` 算，缩图后自动成立）。

顺带的好处：1344² 一帧 4bit 游程约 60–120KB，640² 约 15–35KB，同样 5KB/s 的 BLE，一帧从 15–25s 降到 3–7s。

## 改：底图 ACK 不再硬拦 P3D

之前门槛里有一条 `if !chunkAcked { return "圆屏还没确认底图" }`，实测会长期卡住。原因是固件 type 3 的回执是**延迟 ACK**：

- `handleBulk` 收到 type 3 时把包交给 `queueRasterWork` 增量解码，随后 `pollRasterWork` 解码完才发 `K,3,id`；
- 解码期间若又来了新包 / 主题变了 / 连接 epoch 变了，`cancelPendingRaster()` 直接取消，**这一块永远不会回 ACK**；
- App 侧 `chunkAcked` 就一直是 false，P3D 一帧都发不出去，同时 `checkResend` 每 2s 重灌一整块底图。

现在：ACK 只作提示（设置页黄色小字「圆屏底图还没回执（已 12s），不影响 3D 底图」），不参与放行。理由是 P3D 包自带锚点世界坐标、朝向、俯仰和距离表，固件拿到就能独立重投影，和那张瓦片没有依赖。

## 改：底图补发封顶 3 次

`checkResend` 里同一块底图最多补发 3 次（原来只要没 ACK 就每 2s 无限重发）。ACK 丢一次就让底图常年占住蓝牙队列，P3D 和路线包都挤不进去。重连 / 换块 / 重新收到 ACK 时计数清零。

## 红绿灯距离数字

`#g,<dist>` 那个「下一盏灯 xx m」标签保留。实测（代码口径）：`printOutlined` size 2 是 9 遍描边 × 4 字符 ≈ 7k 次写像素，约 **0.5–1ms**，占 47ms HUD 的 2% 左右，不是帧率瓶颈。真正的大头仍是 P3D/rasterRows3D 重投影（38–64ms）和双环（24ms）。

## 上机顺序

1. 刷固件 `musicA.165`（或更新的 .166），打开串口看 `[P3D]`；
2. 装本版，主题 15/16 + 底图源高德 + 开「原生 3D 底图」；
3. 停车点「立即传一帧」，期望：设置页出现 `3D 底图 #n：xx KB，640×640 90…2680m z17.2`，串口出现 `[P3D] rx #n ...` 与 `[P3D] ok use=...`。
