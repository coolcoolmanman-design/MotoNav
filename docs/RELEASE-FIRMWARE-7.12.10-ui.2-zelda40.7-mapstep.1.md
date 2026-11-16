# MotoNav 固件 7.12.10-ui.2-zelda40.7-mapstep.1

Draft-only · 通用圆屏 1.75 panel0 · 基线 `7.12.10-ui.2-zelda40.6`（`c39a650` / Draft `untagged-0056307c5fb0f7972263`）。

## 相对 zelda40.6

1. **分步换图（cherry-pick 自上游 phone.8 / `c0b56c9`）**  
   - `RasterWork` / `RasterWorkQueue` / stepped `rasterDecodeStep` + `RasterBuildingCarry`  
   - 处理中保留旧图；完成后才切换并回地图 `K,3,id`  
   - 重复末包不提前 ACK；`#BC,0/1` 楼块继承开关

2. **手势**  
   - **右滑** = 电子吧唧页（原天气页槽位）  
   - **首页天气展示保留**（待机天气请求/`wxDirty` 路径不变）  
   - 上划 G 值 / 下划胎压 / 另一侧水平滑音乐不变

3. **电子吧唧**（自 opt.12 最小移植）  
   - BLE bulk **type 7**（拒 type 6）；`CAP,BDG,2`；`#BM,0/1`  
   - 466×466 静图接收+显示；多槽轮播可选

## 未合入

- 上游 phone.8 整仓 UI/pages、导航 `#NW/#NA`、时钟重构  
- opt.12 音乐封面 type5 / Deflate·XOR MD 线  
- 整仓 merge upstream

## 刷机

- 优先 `app_0x10000.bin` @ `0x10000`（保留 NVS）  
- 全新机用 `full_0x0.bin`
