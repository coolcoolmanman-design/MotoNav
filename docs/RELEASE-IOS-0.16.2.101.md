# iOS 0.16.2.101 · musicA.111

FO-261 SoftAP 再修（暖 SoftAP）+ host 协议单测通过后的 Draft。  
**0.16.2.100 实机仍「无法打开圆屏热点」**，本版在模拟门禁通过前不发。

## 本版

- SoftAP：配套 musicA.111 暖 SoftAP（BLE 前 hidden AP 不 stop；`#WIFI,1` 先回执再起服务器）
- App：等待窗 6×3.5s；仍只发 `#WIFI,1`，勿竞态 `#WIFI?`
- Host 证明：`WifiMapBringUpTest` 23 checks（见固件 notes）
- FO-260 自适应旋转随包保留

## 版本

- iOS `0.16.2.101` · build `110`
- FW `7.12.10-ui.2-zelda40.9-musicA.111`

## 自测

1. 刷 musicA.111（分区表 → app）
2. IPA → 小地图 → WiFi 地图加速 → 应出现热点 SSID /「正在加入」/「WiFi 已通」
3. 关掉加速：地图仍 BLE；系统 WiFi 列表不应常驻可见 MotoNav（hidden）
