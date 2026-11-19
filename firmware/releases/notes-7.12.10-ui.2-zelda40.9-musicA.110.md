# MotoNav 固件 v7.12.10-ui.2-zelda40.9-musicA.110

> 配套 iOS App：0.16.2.100  
> 面板：1.75 圆屏（466×466）  
> 基线：musicA.109 / iOS 0.16.2.99（`bfc5423`）

## 本版变更

### FO-260 自适应旋转（四分向朝正）
- 圆屏设置 → 方向：App「自适应旋转」→ `#AR,0/1`；NVS `arot`。
- 板载 QMI8658 重力：相对相位锚定（抵消贴装偏角）+ **仅吸附 0/90/180/270**（dwell 防抖），走现有 `setScreenDeg` / QuarterTurn 路径；不用连续 `rotFree`。
- 近水平（平面重力 &lt; 0.30g）冻结；无磁力计，不声称罗盘/平放偏航。
- 手动 `#R` 自动关自适应并固化角度。

### FO-261 WiFi SoftAP 地图加速再启用
- 恢复 `WifiMapReceiver.h` / `#WIFI,1|0|?` / TCP:7443（可选；关则纯 BLE）。
- **根因**：NimBLE 连上后内部 DRAM 最大连续块常不够 `esp_wifi_init`（musicA.99 所述硬约束）；`.97` 信道重试无法越过。
- **修复**：`setup()` 在 `BLEDevice::init` **之前** `wifiMapPreinitBeforeBle()` 预占 WiFi 驱动；关热点用 `esp_wifi_stop`（保留驱动，勿 `WIFI_OFF`/deinit）。主循环仍最多 6 次退避重试；重试中不发 `WIFI,0,err`。

### 保持不变
- 4MB app 分区表；刷机：先 `0x8000` 分区表，再 `0x10000` app。
- 申花/封面/动态背景等其余功能同 musicA.109。

## 串口确认
- Banner：`MotoNav ready, firmware v7.12.10-ui.2-zelda40.9-musicA.110`
- SoftAP 预 init：`WIFI MAP preinit-ok …`
- 自适应吸附：`[AROT] snap q=…`
