# iOS 0.16.2.136 — 修复样式16天地图瓦片"时有时无"（配 firmware musicA.153）

基线 0.16.2.135。只改 `MapFeeder.swift` 的地图衔接逻辑，**不动固件协议、不动 BLE 传输**。

## 根因

0.16.2.135 把样式 16 纳入了 `buildingMode`（为叠高德建筑轮廓），但地图来源已切到天地图。
于是同一个样式 16 同时被两套逻辑拉扯：既当"纯 HTTP 底图"（天地图 `vec_w`），
又当"高德 SDK 截图建筑任务"（前台双截图）。三处具体后果：

1. **锁屏打断在途瓦片**：`setRenderingAvailable(false)` 里 `if !httpBasemap || buildingMode` —— 
   样式 16 buildingMode=true，即使 `httpBasemap=true`（天地图源）也会执行 `workToken.cancel()`，
   正在下载的 `TdtRaster` URLSession 被取消，已下载的瓦片被丢弃且 `fetching` 复位，
   回前台未必立即补发 → 圆屏瓦片消失一阵。
2. **在途建筑提取无法被移动打断**：`onLocation()` 的 `movement-cancel` 硬编码
   `mapSource == SRC_AMAP`。天地图源下前台低速会走高德建筑双截图（几百毫秒到数秒），
   期间 `fetching=true`，新块 `fetchChunk` 被 `!fetching` 门槛全程阻塞；
   移动又不能取消它 → 旧底图覆盖耗尽后新块补不上。
3. **ACK 丢失后旧包霸占**：`acceptChunk()` 的 `roof-downgrade-blocked` / `base-kept-inflight` /
   `displayBytes == chunkBytes` 三个早退分支，以及 `useCachedMap()` 的同位置早退，
   都只看 `chunkAcked` 当前值、不看发送时长。固件偶发丢 K 后，同一位置的新底图被永久吞掉。

## 修复

- `setRenderingAvailable(false)`：新增 `pureHttp = httpBasemap && (mapSource == SRC_TDT || lockscreenHttpFallback)`，
  纯 HTTP 场景不再因 buildingMode 取消 token。
- `onLocation()` 的 `movement-cancel`：去掉 `mapSource == SRC_AMAP` 限制，
  天地图源的在途建筑提取同样能被移动打断。
- `acceptChunk()`：新增 `ackLost = haveChunk && chunkAcked && 距发送 > 8s`，
  三个早退分支加入该逃逸条件；同字节早退额外要求 `chunkAcked`。
- `useCachedMap()`：同位置早退额外要求 `chunkAcked`，未确认的旧包不挡缓存刷新。

所有改动都是条件收敛，不改变包格式、不改变 type/id 语义、不新增状态机。

## 配对

- 固件 musicA.153；样式 16「昼夜通透」的绘制在固件侧已自洽，本版只解决 iOS 侧供图连续性。
