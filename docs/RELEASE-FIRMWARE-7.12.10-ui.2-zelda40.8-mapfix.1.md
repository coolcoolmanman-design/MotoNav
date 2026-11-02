# MotoNav 固件 7.12.10-ui.2-zelda40.8-mapfix.1

Draft-only · 通用圆屏 1.75 panel0 · 基线 `7.12.10-ui.2-zelda40.7-mapstep.1`（`931c359` / Draft `untagged-ca1858a5fafae0b68d81`）。

## 相对 zelda40.7-mapstep.1

1. **地图空白 / 轮廓 / 网格复测对齐**  
   - 核对上游 `7.12.10-phone.8`（`motonav-2026.11.01-r2`）地图管线：`RasterWork*` / `RasterDecoder` / `RasterBuildingCarry` / `BulkReceiver` 换图调度与 phone.8 一致  
   - 本包固件侧地图解码/分步换图/延迟地图 `K` / `#BC`+`CAP,BLD` 与 phone.8 对齐；修复主因在配套 iOS `0.16.2.1` 地图投喂（MapFeeder/MapGeo 等）

2. **刻意保留（不跟 phone.8 整仓）**  
   - 电子吧唧 BLE bulk **type 7**（拒 type 6）；`CAP,BDG,2`；`#BM`；右滑进吧唧页  
   - TPMS / IMU·G / `#OFF` / 塞尔达开机与 ui.2 壳  
   - 不采用上游 `#NW/#NA` 等待/到达改版

## 未合入

- 上游 phone.8 整仓 UI/pages、时钟重构  
- opt.12 音乐封面 type5 / Deflate·XOR MD 线

## 刷机

- 优先 `app_0x10000.bin` @ `0x10000`（保留 NVS）  
- 全新机用 `full_0x0.bin`  
- 配对：iOS Draft `0.16.2.1-tpms`（地图空白/轮廓/网格主修复在客户端）
