# MotoNav 固件 musicA.86 · 申花断连态 / 抠图 / 略收

> 配套 iOS **0.16.2.80**。基线 musicA.85（解砖）。**musicA.84 仍不安全，勿刷。** Draft 保持。

## 变更

1. **断连态**：中央 idle/connected 跟 `shownConn`；GTA/申花/组合在 `WAIT_LOST` 也画 offline 帧；`#OFF` 后忽略尾随 `P`/`#TIME`；`C` 清屏按链路回 ready/no-link。
2. **申花中央重烘焙**：189×135（≈90%）、软 alpha、去导引线；开机 AA 未改。
3. Flash：本地 arduino-cli `Sketch 3133139` · `app.bin 3133392` · 分区余量 **12336 B**（≥1.5 KiB；相对 musicA.85 约 +10 KiB）。

## 刷机

`…_panel0_app_0x10000.bin`（保留 NVS）。**勿刷 musicA.84。**
