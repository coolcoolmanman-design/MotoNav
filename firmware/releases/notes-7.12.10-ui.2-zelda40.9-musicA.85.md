# MotoNav 固件 musicA.85 · Flash 超限紧急修复

> 配套 iOS **0.16.2.79**。基线 musicA.84（**不安全，勿刷**）。Draft 保持。

## 紧急

musicA.84 实机 **Image length 3145920** 不进分区 **3145728**（+192 B）→ 砖机循环重启。  
恢复：**musicA.83** https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-3d4e84f98f8c549f1078 的 `…_panel0_app_0x10000.bin`。

## 变更

- ST_DRAGON：去掉光点表 / 四星 ping / 网格 / placeEdgeBall 搜索；保留**极简单前缘扫线** + 四星球点。
- 若干调试 Serial 字符串缩短（省 Flash）。
- 申花 large / 开机 AA / 等红灯未改。
- 本地 arduino-cli：`Sketch 3143911` · `app.bin 3144160` · 分区余量 **1568 B**（≥1.5 KiB）。

## 刷机

`…_panel0_app_0x10000.bin`（保留 NVS）。**勿再刷 musicA.84。**
