# iOS 0.16.2.87 · musicA.97

修 **无法打开圆屏热点**：开「WiFi 地图加速」后圆屏 SoftAP 在 BLE 共存下常首次失败，App 又立刻 `#WIFI?` 把重试中的状态当成永久 `WIFI,0,err`。Draft-only。

## 本版

- SoftAP 打开改为只发 `#WIFI,1`，给固件重试窗口；失败才最终显示「无法打开圆屏热点」。
- 加入 `Hotspot Configuration` / Wi‑Fi info entitlements，改善 NEHotspot 自动加入。
- 配套固件 **musicA.97**：SoftAP 关干净再开、modem sleep、ch1/ch6 交替、最多 5 次主循环重试，用尽才回 `WIFI,0,err`。

## 版本

- iOS `0.16.2.87` · build `96`
- FW `7.12.10-ui.2-zelda40.9-musicA.97`
