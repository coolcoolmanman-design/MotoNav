# musicA.164

样式 16「昼夜通透」两处版式修正 + 移除 WiFi 地图加速。

## 1. 倒计弧按参考图改成「左侧单段」（.162 的跨顶 218° 作废）

- 参考图实测（夜间稿最干净）：光柱从 **-140°（8 点下方）** 起，沿左半环上行到 **-14°（11 点半）** 止，跨度 **126°**；12 点整段留白给时间（"02:20/22:18" 字面 x206..270 / y33..48），**弧不过顶**，右半环既没有槽也没有第二条光柱。
- `.162` 的「越过顶部、12 点两侧各 109°」是把真机拍摄的边缘反光读成了弧：夜间参考图右半环的径向剖面在 R216~222 处没有任何槽/柱的暗位。
- 槽与光柱都改成单条 `arcBand`（126° < 180°，不再需要 12 点处两段重叠防缝的补丁），绘制开销从 4 条降到 2 条。
- 头端粉点仍跟着 `ringShown` 走：左下起步 → 11 点半收尾；≤120m 呼吸节奏不变。

## 2. 底部胶囊正下方补电量图标（A 方案）

- 位置 x210..256 / y428..452，在「14分 · 6.2km」胶囊正下方居中；46×24 圆角矩形，2px 浅蓝灰描边 (150,200,210) + 内部深青压底 + 数字居中。
- 数字用 `GA_F` 档（cap 16）走**独立软阴影槽** `GA_SM_BAT = 7`（`SM_N` 7 → 8）。和时间/单位/大距离共槽会每帧互相冲掉掩膜，重演 `.157` 的 10ms 开销。
- 状态色：充电 = 亮青描边 + 青字；<20% = 橙色描边 + 橙字；其余浅蓝灰 + 白字。没接电池（`batPct < 0`）不画。

## 3. 删除 WiFi 地图加速（SoftAP + TCP 旁路）

不好用，且会额外占用内部 RAM 导致重启；后续要用再重新编。

- 整文件删除：`src/connectivity/WifiMapReceiver.h`、`src/connectivity/WifiMapProtocol.h`、`tests/WifiMapBringUpTest.cpp`。
- 命令处理：`#WIFI,1 / #WIFI,0 / #WIFI?` 三条；`#BC`（建筑开关）与 `#t`（换风格）里的 `wifiMapCancelPendingTransfer()` 两处。
- 回执：`Notifications.h` 的 `pendingWifiStatus` 状态回执块。
- 内存抑制机制：`RadarEffects.h` 的 `wifiMapNoInternalCaches` 标志、`wifiMapReclaimInternalHeap()` / `wifiMapRestoreInternalCaches()`，以及它在 `ringTablesInit()`、`prepareRingFastColors()`、`drawVignette()` 里的三处抑制判断——雷达环半径表（≈5.8KB）、`ringFastColors`（≈12.5KB）、`vigKeep`（≈12.9KB）恢复为**始终按需分配**。
- `setup()` 的 `wifiMapPreinitBeforeBle()`、主循环的 `wifiMapTick()`、`WIFI_MAP_SELFTEST` 自测块、`FunctionDeclarations.h` 的 8 个声明。

## 兼容

- App 侧 `#WIFI,*` 命令将不再被识别（固件静默忽略，无回执）；iOS 仍保留开关，需要的话后续版本一并删除。
- 协议、bulk 类型、NVS 布局无变化，可直接覆盖刷写。
