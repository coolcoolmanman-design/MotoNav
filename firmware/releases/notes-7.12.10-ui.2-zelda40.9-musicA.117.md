# musicA.117 — WiFi 地图加速：把 WiFi 驱动挪到蓝牙之前初始化（真修复）

## 症状
App 打开「WiFi 地图加速」始终提示「无法打开圆屏热点，请重试」。
串口实测：BLE 连上之后内部 RAM 装不下 `esp_wifi_init`；第一次失败回滚不干净
（`Failed to deinit Wi-Fi driver (0x3001)`），之后每次只剩 `largest≈7.2KB`，再也起不来。

## .116 为什么还不够
`.116` 的 reclaim 在 BLE 连接态释放 0 字节：`ringFastColors` 根本没分配过
（要内部空闲 >45KB 才分配），`vigKeep` 不在待机页，`rq2r` 也不在。
换句话说，**BLE 连上后的内部堆怎么腾都不够**，运行时 `esp_wifi_init` 是死路。

## .117 真修复（已在实机串口验证）
1. **三块大缓存搬到 PSRAM**：BLE 接收队列 `rxq` 24.6KB、像素城画布
   `pxGrid`+`pxMask` 18.3KB、音乐背景 `mbgGrid` 8KB，共腾出
   **51,096 字节内部 RAM**（静态占用 166,892 → 115,796）。
   这三块都不在每像素热路径的核心位置，实测显示正常。
2. **开机在 `BLEDevice::init()` 之前预初始化 WiFi 驱动**（此时内部堆连续），
   然后只 `esp_wifi_stop()` 停射频，**驱动常驻**。收到 `#WIFI,1` 时只需
   `esp_wifi_start()` + `WiFi.softAP(...)`，不再做 `esp_wifi_init`。
   绝不用 `softAPdisconnect(true)` / `WiFi.mode(WIFI_OFF)` —— 那会
   `wifiLowLevelDeinit()` 把驱动卸掉（`.110` 就是栽在这里）。
3. **只给用过的设备预初始化**：NVS `motonav/wifi_ap`。没用过加速的设备
   （默认）开机完全不走这段，行为不变。
4. **首次打开自动重启一次**：本 boot 没有预初始化时收到 `#WIFI,1`，写入
   `wifi_ap=true`、回 `WIFI,0,err`（App 重试窗口内会继续重发）、然后重启。
   重启后驱动在蓝牙之前装好，App 重连补发 `#WIFI,1` 即可直接开热点。
5. **关热点保留驱动**：`#WIFI,0` 只停射频，同一次开机内可反复开关。

## 实机日志（COM5，按住加速开关验证）
```
[MEM] psram-globals int_free=216360 largest=155636
WIFI MAP pre-ble int_free=181900 largest=139252
WIFI MAP preinit-ok int_free=135500 largest=94196
[MEM] after BLE init int_free=63868 largest=31732
[MEM] ready int_free=37816 largest=18420
WIFI MAP bringup int_free=37160 largest=18420
WIFI MAP SoftAP ssid=MotoNav-4504 ip=192.168.4.1 tcp=7443
WIFI MAP running int_free=28956 largest=18420
WIFI MAP SoftAP off (driver kept)
WIFI MAP SoftAP ssid=MotoNav-4504 ip=192.168.4.1 tcp=7443
```

## 代价
- 用过加速的设备：开机多约 0.3 秒，WiFi 驱动常驻占用少量内部 RAM（射频关闭，
  不耗电、不影响蓝牙）。
- 没用过加速的设备：仅有三块缓存改走 PSRAM（PSRAM 余量充足），无其它变化。
- 首次打开加速需要重启一次；之后开机即用，不再重启。
