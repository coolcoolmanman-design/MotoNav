# iOS 0.16.2.100 · musicA.110

双功能 Draft：圆屏自适应旋转（FO-260）+ 再启用 WiFi SoftAP 地图加速（FO-261）。基线 `ios-v0.16.2.99` / musicA.109（`bfc5423`）。Draft-only。

## 本版

### FO-260 自适应旋转
- 圆屏设置 → 方向：新增「自适应旋转」开关（UserDefaults `auto_rotate` + `#AR,0/1`）。
- 开：固件 QMI8658 重力吸附 0/90/180/270；手动圆盘停用。
- 关：恢复 RotationDial 手动 `#R`。
- 文案不声称罗盘/平放偏航。

### FO-261 WiFi SoftAP 地图加速
- `WifiMapLink.featureEnabled = true`；设置页恢复「WiFi 地图加速（试验）」；失败仍 BLE。
- 只发 `#WIFI,1` 等成功回执；重试窗口加长（5×3.2s）；最终失败「无法打开圆屏热点」。
- 配套固件 musicA.110：BLE 前预 init WiFi 驱动，修复 DRAM 硬约束导致的热点打不开。

## 版本

- iOS `0.16.2.100` · build `109`
- FW `7.12.10-ui.2-zelda40.9-musicA.110`

## 刷机 / 自测

1. 分区表 `0x8000` → app `0x10000` 刷 musicA.110（勿用 `full_0x0` 除非可清 NVS）
2. 装本 Draft IPA，BLE 连圆屏
3. 方向 → 开「自适应旋转」：竖持转屏应四分向朝正；近水平应冻结
4. 小地图 → 开「WiFi 地图加速」：应出现热点 SSID /「WiFi 已通」，**不应**「无法打开圆屏热点」；关掉后地图仍 BLE

## 兼容

- 新 App + 旧固件：`#AR` / `#WIFI` 被忽略；手动方向与 BLE 地图正常。
- 旧 App + 新固件：自适应默认关（NVS）；SoftAP 不主动开。
