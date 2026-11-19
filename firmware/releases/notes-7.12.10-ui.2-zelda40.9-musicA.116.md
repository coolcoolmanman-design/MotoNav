# musicA.116 — WiFi 地图加速：让 reclaim 真的腾出连续块

## 症状
App 打开「WiFi 地图加速」始终提示「无法打开圆屏热点，请重试」。
不是 iOS 的 UI 问题：固件侧 `WiFi.mode(WIFI_AP)` 失败（`esp_wifi_init` 拿不到足额
内部连续 RAM），iOS 等 24 秒超时后报这句。

## .115 为什么没解决
- `.115` 的 `ringFastColors`（12.5KB）只有在内部空闲 > 字节数 + 32KB（≈45KB）
  时才会分配。BLE 连接态内部只剩 32.7KB，**它压根不存在**，reclaim 释放 0 字节。
- 失败路径会调用 `wifiMapRestoreInternalCaches()`，把 `ringFastTried` 复位，
  雷达下一帧立刻把 12.5KB 吃回去，越试越差。
- App 每 3.5 秒重发 `#WIFI,1`（共 7 次），每次都清零计数并立刻再做一次
  `esp_wifi_init`。24 秒内约 7 次 init/deinit，把内部堆搅得更碎。

## .116 改动
1. **reclaim 加 `rq2r`**：进度环半径查找表 ≈5.8KB（`ringTablesInit` 按需重建）。
   加速窗口内 `ringTablesInit()` 直接返回 false，走既有软件降级绘制路径，
   不会下一帧又吃回去。
2. **恢复缓存只在用户主动关热点时发生**：`wifiMapStop()` 拆成
   `wifiMapStopInternal(restoreCaches)`，服务器分配失败等内部路径不再恢复雷达缓存。
3. **挡住重试风暴**：放弃后 15 秒冷却，冷却期内的重复 `#WIFI,1` 直接回
   `WIFI,0,err`，不再 init/deinit；固件自己的退避窗口开着时，重复命令直接忽略。

## 预期
- 串口 `WIFI MAP after-reclaim ... largest=` 应比 `.115` 的 18420 更大
  （ringFast 若存在 +12.5K，`rq2r` +5.8K，vigKeep +12.9K；实际增量取决于缓存是否存在）。
- 若 `largest` 仍 < 门槛，日志会出现 `mode(AP) fail`，且 15 秒内只有一轮尝试，
  不再有 7 连 init。

## 代价
- WiFi 加速期间：雷达进度环走软件绘制、无 fast colors，vignette 走软件路径；
  停止热点（`#WIFI,0`）后恢复原状。
- 不使用 WiFi 地图加速的用户完全无变化。

## 下一步（若仍未过门槛）
把 `esp_wifi_init` 提前到 `BLEDevice::init()` 之前（那时 largest≈38.9KB，`.110` 实测可过），
`#WIFI,1` 时只做 `set_mode(AP)` + `start`，不再 init。
