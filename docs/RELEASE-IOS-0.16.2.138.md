# iOS 0.16.2.138

配套固件：**musicA.164+**（.164 已删除固件侧 WiFi 地图加速）。

## 1. 删除 WiFi 地图加速（iOS 侧）

固件 .164 已把这条链路整段删掉（lwIP/WiFi 协议栈裁掉后 app 小 557 KB，也不再吃内部 RAM 导致重启），iOS 侧留着只剩一个永远连不上的开关。

- 删除文件：`WifiMapLink.swift`（517 行），并从 `project.pbxproj` 摘掉 BuildFile / FileReference / group / Sources 四处。
- `BleLink`：去掉 `WIFI` 通知转发（`onNotify`）。
- `MapFeeder`：去掉 `evaluateEmergency` / `clearBleOnlyIfNeeded` / `putRaster` / `shouldPreferWifi` / `enterBleOnly` / `markChunkAcked` / `transferActive` / `recentlySentViaWifi` 的全部调用，以及 `onWifiMapReady()` / `onWifiPathLost(reason:)` 两个只由 WifiMapLink 触发的回调。
- `sendChunk()` 恢复成纯 BLE 一条路；`checkResend()` 的底图等待从「WiFi 8s / BLE 2s」统一成 2s；`shortStatus` 去掉「WiFi 整包 · xxKB」分支。
- `Settings.wifiMapBoost` 与设置页「WiFi 地图加速（试验）」整块消失；`UserDefaults` 里的旧键不再读取（不回写）。
- 净效果：底图路径只剩 BLE 分包，代码量与状态机显著变少。

## 2. 3D 底图「现在发不出去：…」

`MapFeeder.perspGate()` 把发一帧 P3D 的全部门槛按顺序摊平成一句话，设置页开关下方直接显示，例如：

- `开关未开` / `小地图已关闭`
- `底图源是天地图（P3D 只走高德）`
- `主题 14 不是 15/16`
- `App 不在前台（锁屏不抓图）`
- `还没抓到首张底图` / `圆屏还没确认底图`
- `正在抓新底图` / `有底图在排队待发`
- `蓝牙队列忙（底图/路线在传）`
- `离块中心 132m（≥160m 就换块）`
- `距上一帧 3s，要 ≥7s`
- `没挪 40m（12m）也没转 10°（2°）`

顺序按「最容易踩到」排，所以第一行就是真正的拦路条件。空 = 此刻没有拦截（满足条件就会发）。

## 3. 「立即传一帧」

设置页同一位置新增按钮，跳过 `7s / 挪 40m / 转 10° / 块中心` 四个节流，其余门槛照旧（仍要求前台、圆屏已确认底图、蓝牙空闲）。停车调试专用：不用真开出去 40m 也能出一帧。发完按钮下方会显示 `3D 底图 #n：xx KB，448×448 90…2680m z17.2`，圆屏回执后 `perspAcked` 置位。

## 4. 诊断分工

| 现象 | 看哪里 |
|---|---|
| `现在发不出去：xxx` | App 侧被哪条门槛挡住 |
| 有 `3D 底图 #n` 但圆屏还是瓦片 | 固件侧：串口 `[P3D]` 行（musicA.165+） |

固件 `[P3D]` 会打印 `rx #n 448x448 lut=64 …`（收到）、`reject: <原因>`（解析失败）、`ok / no-frame / stale-bearing`（每帧可用性），详见 `notes-…-musicA.165.md`。

## 兼容

协议、bulk 类型、NVS 键、地图抓图链路均无变化。0.16.2.137 → 0.16.2.138 可直接覆盖安装。
