# musicA.113

## 变更

### FO-261 修复：回退 `.96` 干净路径，解决 `0x3001` deinit 循环导致热点起不来

`.110`/`.112` 已在 COM5 实测：BLE 连上后 `#WIFI,1` 反复打印

```
E (...) wifi_init: Failed to deinit Wi-Fi driver (0x3001)
WIFI MAP mode(AP) fail (bringup)
```

内部堆从 bring-up 时的 `largest≈18420` 掉到 `7156`，6 次重试全部失败后放弃。

根因不是「BLE + SoftAP 内存不够」（用户指出 `.96`/`.98` 旧固件能正常开热点），而是 `.110` 引入的 **preinit「装驱动再拆」**：开机在 `BLEDevice::init()` 前先 `WiFi.mode(AP)` 再 `softAPdisconnect`+`esp_wifi_stop()`，把驱动置于半初始化状态；bring-up 时再调 `WiFi.mode(WIFI_AP)` 触发 Arduino 内部 deinit→reinit，对半死驱动 deinit 失败（`0x3001`），反复重试持续泄漏堆。

`.113` 的修法：

- **取消 preinit**：删掉 `wifiMapPreinitBeforeBle()` 及其在 `setup()` 中的调用。BLE 正常初始化，不再有半死驱动。
- **bring-up 回到 `.96` 干净路径**：`#WIFI,1` 时单次 `WiFi.mode(WIFI_AP)` + `softAP()` 一次性装驱动起热点；关热点用 `WiFi.mode(WIFI_OFF)` 彻底 deinit，不留残留。
- **保留内存回收**：bring-up 前 `wifiMapReclaimInternalHeap()` 释放导航页可选缓存 `vigKeep`（~12.9KB）+ `ringFastColors`（~12.5KB）——真实导航态下正是它们把最大连续块从 ~18KB 压到 ~5KB，让 WiFi 驱动 init 装不下。这是相对 `.96` 的唯一增强，针对本机导航态场景。
- **删除测试钩子**：移除 `.112` 的 `wifiMapSimNavCache()`（在 bring-up 时才申请缓存、方向错误）及 `wifiMapWanted=true` 自动热点开关，恢复由 App `#WIFI,1` 真实触发。

### 验证目标
- 开机无断言、无反复重启，BLE/CFQ/TIME 正常。
- 进入导航态后 `#WIFI,1` 能一次起热点（不再 `0x3001`），`WIFI,1,...` 回执正常，TCP 灌图拿到 `K,3,id`。
- 若仍失败，串口 bringup/after-reclaim/mode-fail 的 heap 日志用于定位下一步。

## 保持
- FO-260 自适应旋转同 `.110`/`.112`。
- 4MB app 分区；分区表 magic `aa50`。

## 刷机
- 分区表 `0x8000` + app `0x10000` 一起刷；**不要刷 `full_0x0.bin`**（抹 NVS）。
- app 槽上限 4,194,304B，刷前核对 manifest SHA256。

## 配套 App
- iOS 0.16.2.103：仅版本号递增与通用固件段更新为 `.113`。
