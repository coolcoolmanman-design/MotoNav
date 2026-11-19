# musicA.114 — WiFi 地图加速：内部 RAM 结构性腾退

## 背景
- musicA.113（回 .96 干净路径、无 preinit）实测：BLE 连接态首次 bringup 堆干净
  （int_free=32712 largest=18420）仍 `WiFi.mode(AP)` 失败 0x3001，失败后堆泄漏 17.7KB。
- 结论：preinit 不是根因，**内部 RAM 连续块不足**才是。历史能开热点的
  v7.12.10-wifi.2 app 仅 1.8MB；现行 3.6MB 的功能把内部堆吃光。

## 修复（固件）
1. **共享解压器**：6 个独立 tinfl_decompressor（开机动画/地图/封面/吧唧/背景/封面流，
   各 ≈11KB，合计 ≈66KB 内部 RAM）合并为 `motonavSharedInflater()` 单例（省 ≈55KB）。
   依据：全部调用点主循环单线程、每次解压前 tinfl_init 重置、一次性完整解压。
2. **PSRAM 化**：车道胶囊 laneCol/laneA（42KB）、地图 tmpX/tmpY（32KB）改 PSRAM 优先
   （省 ≈74KB 内部）。
3. bringUp 前释放导航可选缓存（vigKeep 12.9K / ringFast 12.5K）保留。

## 预期
- BLE 连接态 int_free 32.7KB→≈130KB，largest 18.4KB→≈70KB+，SoftAP init 可成功。
- 验证：App 开 WiFi 加速 → 串口应见 `WIFI MAP softAP ok`，App 不再「重试中」。

## 兼容性
- 解压功能（开机动画/封面/地图/吧唧/动态背景）逻辑不变，仅共用同一 11KB 实例；
  每次解压前 tinfl_init 重置，无状态残留风险。
- 车道/地图绘制读 PSRAM 略慢（每帧约 1-2ms），30fps 余量充足。
