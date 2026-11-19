# iOS 0.16.2.102 · musicA.112

修复 `.111` 启动断言重启循环的配套包。Draft-only。App 本身功能同 0.16.2.101，仅版本号递增与通用固件段更新。

## 本版

- **配套固件 musicA.112**：WiFi SoftAP 改为「驱动暖、射频按需」——BLE 前只初始化 WiFi 驱动并停射频，BLE 后再按需起热点，消除 `.111` 的 `xQueueGenericSend` 断言重启，同时绕开 `.110` 的 `0x3001` 二次 init 失败。
- `#WIFI,1` 控制面协议不变：先回 `WIFI,1,...`，再起 HTTP/TCP；重试中不误报 err。
- FO-260 自适应旋转、`#IDLEOFF` 等功能不变。

## 版本

- iOS `0.16.2.102` · build `111`
- FW `7.12.10-ui.2-zelda40.9-musicA.112`

## 刷机

1. 分区表 `0x8000` → app `0x10000` 刷 musicA.112
2. 装本 Draft IPA
3. 圆屏开机 banner 应为 `MotoNav ready, firmware v7.12.10-ui.2-zelda40.9-musicA.112`

## 兼容

- 新 App + 旧固件：旧命令照常；`#WIFI` 在旧固件表现依其版本而定。
- 旧 App + 新固件：动画与 BLE 正常；WiFi 加速需支持 `#WIFI` 的 App 版本。
