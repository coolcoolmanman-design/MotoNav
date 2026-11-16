# 固件 7.12.10-ui.2-zelda40.9-musicA.65

基线 musicA.63 / iOS 0.16.2.60。**固件小 Draft**（FO-210 + FO-211）；IPA 仍用 **0.16.2.60**。
（musicA.64 CI 未含 WX 符号时已升 .65 同包落地，避免 Draft 标签冲突。）

- FW Draft：https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-7c7d805db8f10e78b4eb
- tip：`0f22d29`
- IPA：仍用 0.16.2.60（https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-ffde7765b10d86fd84dd）

## FO-210 · Infograph IG_SCALE 对齐 BASE

`IG_SCALE` → `1.2469f`（与 BASE DIAL_SCALE 一致），消除四角多余灰轨 / 叠层观感；缓存擦净加固。

## FO-211 · 左子盘 WX 天气符号（和风）

- 复用 `#W,<icon>,…` → `wxIcon` + `drawWxIcon`（0–11）
- 左子盘画天气符号，不再显示温度（气温在右上弧）

## 保留 FO-208 / FO-209

## 建议实机验证

1. 升 FW musicA.65（IPA 0.16.2.60）→ C Infograph
2. 灰轨嵌合彩弧；左 WX 为符号非温度；划页不卡死
