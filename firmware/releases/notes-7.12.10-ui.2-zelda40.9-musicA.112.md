# musicA.112

## 变更

### FO-261 v4：修复 `.111` 启动反复重启，SoftAP 改为「回收内存 + 干净重初始化」

`.111` 的暖 SoftAP 方案在 `BLEDevice::init()` 之前就把 SoftAP 射频跑起来，实机每次开机触发：

```
assert failed: xQueueGenericSend queue.c:936 (pxQueue)
```

NimBLE 初始化所需队列被正在运行的 WiFi SoftAP 抢占，无限重启、BLE 完全不可用。

`.112` 从已实测稳定的 `.110` 架构重建，只改热点启动路径：

- **保留 `.110` 的 preinit**：BLE 前 `mode(WIFI_AP)` 置位 Arduino `lowLevelInitDone` → `softAPdisconnect(true)` 拆 SoftAP 缓冲 → `esp_wifi_stop()` 停射频。BLE 侧实测正常。
- **bring-up 前回收内部 RAM**：`wifiMapReclaimInternalHeap()` 释放导航页可选缓存 `vigKeep`（~30–54KB）+ `ringFastColors`（~12KB），给 BLE 连接态下的 WiFi 驱动重新初始化腾出连续块。这两块只在进导航页后分配，正是「开 WiFi 加速灌地图」的典型场景。
- **去掉 `.110` 的错乱 bring-up**：旧代码在 `driverReady=true` 时先 `esp_wifi_start()`（驱动已被 preinit 拆掉 → `0x3001`），又二次 `WiFi.mode(WIFI_AP)`，状态机错乱。新版直接走单次干净的 `WiFi.mode(WIFI_AP)` + `softAP()`。
- **控制面协议不变**：`WIFI,1,...` 先回执再起服务器；重试中不误报 `WIFI,0,err`；用尽重试才报错。

### 验证
- 主机模拟 `scripts/wifi_map_sim.py`：22 项检查通过。
- 目标：实机启动无断言、BLE/CFQ 正常；`#WIFI,1` 能起热点且 TCP 灌图拿到 `K,3,id`（刷机后实测）。

## 保持
- FO-260 自适应旋转同 `.110`/`.111`。
- 4MB app 分区。

## 刷机
- 分区表 `0x8000` + app `0x10000` 一起刷；**不要刷 `full_0x0.bin`**（抹 NVS）。
- app 槽上限 4,194,304B，刷前核对 manifest SHA256。

## 配套 App
- iOS 0.16.2.102：仅版本号递增与通用固件段更新为 `.112`。
