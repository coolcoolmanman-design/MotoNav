# iOS 0.16.2.103 · musicA.113

修复 `.110`/`.112` WiFi 热点 `0x3001` deinit 循环的配套包。Draft-only。App 本身功能同 0.16.2.102，仅版本号递增与通用固件段更新。

## 本版

- **配套固件 musicA.113**：回退 `.96` 干净路径——取消 preinit「装驱动再拆」，`#WIFI,1` 时一次性 `WiFi.mode(AP)+softAP()` 起热点，关热点彻底 `WIFI_OFF`；bring-up 前释放导航可选缓存腾出连续内部块。解决 `.110`/`.112` 反复 `Failed to deinit Wi-Fi driver (0x3001)` 导致热点起不来的问题。
- `#WIFI,1` 控制面协议不变：先回 `WIFI,1,...`，再起 HTTP/TCP。
- FO-260 自适应旋转、`#IDLEOFF` 等功能不变。

## 版本

- iOS `0.16.2.103` · build `112`
- FW `7.12.10-ui.2-zelda40.9-musicA.113`

## 刷机

1. 分区表 `0x8000` → app `0x10000` 刷 musicA.113
2. 装本 Draft IPA
3. 圆屏开机 banner 应为 `MotoNav ready, firmware v7.12.10-ui.2-zelda40.9-musicA.113`

## 兼容

- 新 App + 旧固件：旧命令照常；`#WIFI` 在旧固件表现依其版本而定。
- 旧 App + 新固件：动画与 BLE 正常；WiFi 加速需支持 `#WIFI` 的 App 版本。
