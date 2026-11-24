# iOS 0.16.2.128

- **关闭导航中「应急自动开 WiFi 地图加速」：**
  - `WifiMapEmergencyPolicy.shouldEngage()` 顶部 `guard emergencyWiFiEnabled else { return false }`，默认关闭；高速 / 底图空白 / 传图未完不再自动拉起 SoftAP。
  - 根因：导航首帧触发 `#WIFI,1` 会让固件主动重启一次（musicA.117 的 WiFi 驱动 preinit 机制），且本板 BLE + SoftAP 不可兼得。关闭后底图全程走 BLE，**首次导航不再重启**。
  - 保留手动开关：设置里的「WiFi 地图加速（试验）」打开时仍走 `requestSoftAP`，不受影响。
  - 代价：高速或底图将空白时不再有 WiFi 加速兜底，仅靠 BLE 传图。
- **保留 0.16.2.127 / .126 全部修复：**首页清晰设备图（卡高不变、不压搜索框）、路口放大图调色不再越界闪退（`bestHue`/`bestCnt` 分离）、零尺寸守卫。
- 基线：`cursor/ios-home-device-clear-127` @ 0.16.2.127。
- 配对固件：musicA.140 及以上。

# retrigger: iOS 0.16.2.128 disable emergency auto WiFi
