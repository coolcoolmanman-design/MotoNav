# iOS 0.16.2.86 · musicA.95

回归 **WiFi 地图加速（试验）**：圆屏 SoftAP + TCP 7443 整包传 type3 底图，BLE 仍负责控制/路线/确认。Draft-only。

## 本版

- 小地图设置新增「**WiFi 地图加速（试验）**」开关（默认关）+ 热点连通状态行。
- WifiMapLink：SoftAP 自动加入（NEHotspot）、HTTP 探测、TCP 整包 latest-wins、失败 BLE-only 冷却 45s。
- **应急热点**：导航中 ≥70km/h（卫星 50）或空白风险自动临时开热点，传图未完不关。
- MapFeeder：type3 优先 WiFi 整包，失败/丢路径立刻 BLE 兜底；WiFi 在途时重发等待 8s。
- Info.plist：NSLocalNetworkUsageDescription + NSAllowsLocalNetworking。
- 配套固件 **musicA.95**：`#WIFI,1/#WIFI,0/#WIFI?` 命令 + WifiMapReceiver（SoftAP+TCP）。

## 版本

- iOS `0.16.2.86` · build `95`
- FW `7.12.10-ui.2-zelda40.9-musicA.95`
