# iOS 0.16.2.93 · musicA.102

龙珠雷达 v3 + 原版/极简黑白天地图空白修复 + 主题 12/13 持久化。Draft-only。SoftAP 仍关；天地图+高德轮廓混合与预取保留。

## 本版

- **FO-252 龙珠雷达 v3**（配套固件）：恢复笛卡尔方格十字网（step=40）+ 亮前缘/指数软拖尾扇扫；对齐确认稿 `docs/dragonball-radar-sweep-v3-mock.md`
- **FO-253 原版(0)/极简黑白(5) 空白**：`MapStyleTransfer` 接受天地图 classified 包；style 5 remask 为道路掩膜；TDT 取图/预取/accept 路径均 `adapt`
- **FO-254**：冷启动主题 allowlist 加入 **12 / 13**（大字转向、矢量简图重启不丢）

## 版本

- iOS `0.16.2.93` · build `102`
- FW `7.12.10-ui.2-zelda40.9-musicA.102`

## 刷机

1. 分区表 `0x8000` → app `0x10000` 刷 musicA.102
2. 装 IPA **0.16.2.93**
