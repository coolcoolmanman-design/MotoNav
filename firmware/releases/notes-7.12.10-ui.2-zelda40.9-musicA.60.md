# 固件 7.12.10-ui.2-zelda40.9-musicA.60

基线 musicA.59 / iOS 0.16.2.57（`3edbd1c` · Draft
`untagged-de553523f33f628fba79`）。配套 iOS **0.16.2.58**。

## 一、组合动画与中央时间重叠

根因：`animateCat` 里 `comboBuiltin` 只看 `vehId==4 && comboUseBuiltin`，
未像 GTA/CSP 那样要求 `!centerTime` / `slotMask bit3`。切到中央=时间后
仍每 100ms 叠画组合，压在 `drawCenterClock` 上。

修复：`comboBuiltin` 与 `cspTop`/`gtaTop` 同门控——中央=时间或隐藏时不画组合。

## 二、Infograph C 收边 + 电量避北子盘

- 烘焙底图与叠层几何相对 07v2 ×**1.14**（白盘/四弧/子盘/扁针同步放大收边）
- 针心电量改 `CX-33`（≈y200，对齐确认稿落地），不再用 A/B 的 `CX-78`（会压 SPD）
- A/B 全屏指针电量位置不变；外标径向钳 ≤224 防裁边

## 建议实机验证

- 中央：组合 → 时间：组合消失，只剩时间；再切回组合恢复
- C Infograph：重下发底图后黑边收窄；电量在针心上方、不挡北子盘 SPD

## 残影带

中央=时间时对 vehId 3/4 仍 `zbgRect(0,100,466,300)` 清组合/GTA 像素。
