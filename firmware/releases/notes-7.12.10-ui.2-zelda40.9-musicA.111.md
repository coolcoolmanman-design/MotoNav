# MotoNav 固件 v7.12.10-ui.2-zelda40.9-musicA.111

> 配套 iOS App：0.16.2.101  
> 基线失败包：musicA.110 / iOS 0.16.2.100（实机仍「无法打开圆屏热点」）

## 本版变更（FO-261 v3）

### 实机失败证据
- Draft `untagged-f0d9a307b1b46f8434de` / FW `untagged-e4be65d99cbe566dc797`
- 开「WiFi 地图加速」→ 状态「无法打开圆屏热点」（截图见 store `media/wifi-softap-hotspot-fail-0.16.2.100.jpg`）
- `.110` 的 BLE 前 `esp_wifi_init` + `esp_wifi_stop` **不够**：BLE 起来后再 start/softAP 仍失败

### 修复：暖 SoftAP
- `wifiMapPreinitBeforeBle()`：BLE 前 `WiFi.mode(AP)` + **hidden softAP**，**不** `esp_wifi_stop`
- `#WIFI,1`：unhide/重配 → **先回 `WIFI,1,...`** → 再起 HTTP/TCP
- `#WIFI,0`：停服务器并重新 hidden，保持暖射频（BLE-only 地图仍可用）
- SoftAP 前 `wifiMapReclaimInternalHeap()` 释放 ring/vignette INTERNAL 缓存

### Host 模拟证明（Draft 前门禁）
- `firmware/tests/WifiMapBringUpTest.cpp` + `WifiMapProtocol.h`
- 覆盖：暖 enable→WIFI,1；重试中 query 不报 err；用尽才 WIFI,0,err；disable→WIFI,0；App 时间窗内必得 WIFI,1
- CI / 本地：`g++ -std=c++17 firmware/tests/WifiMapBringUpTest.cpp && ./a.out` → 23 checks passed

### 保持
- FO-260 自适应旋转同 .110
- 4MB 分区；刷机先 `0x8000` 再 `0x10000`
