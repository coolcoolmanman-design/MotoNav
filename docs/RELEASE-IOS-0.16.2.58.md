# iOS 0.16.2.58

配套固件 `7.12.10-ui.2-zelda40.9-musicA.60`。基线 0.16.2.57
（`3edbd1c` · Draft `untagged-de553523f33f628fba79`）。

## 变更

- Infograph 烘焙底图 / 预览换成 **×1.14 收边** 版（`InfographDialBase` /
  `InfographDialPreview`）；选 C 或点「重新下发底图」推新壁纸
- 组合→时间重叠、Infograph 电量挡 SPD 均在固件侧修复（见配套 FW notes）

## 建议实机验证

- 升级后切 C Infograph 并重新下发底图：表盘更满、电量不挡 SPD
- 中央组合动画 ↔ 时间：互不残留叠画

含中央=时间残影带 wipe（`7114882` 起）。
