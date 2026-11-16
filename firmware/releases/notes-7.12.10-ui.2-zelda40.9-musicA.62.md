# 固件 7.12.10-ui.2-zelda40.9-musicA.62

基线 musicA.61 / iOS 0.16.2.59（`ba620fe`）。**固件小 Draft**（FO-209）；IPA 仍可用 0.16.2.59。

## FO-209 · Infograph 全屏指针触摸卡死

**现象**：仅 C Infograph 全屏指针页触摸无效且卡死，无法划到音乐/吧唧/G/胎压；其他页正常。

**根因**：每秒 `tickFullFace` 重跑三段 `igArcGrad`→大量 `arcBand`（抗锯齿弧带），主循环被拖死，`touchPoll` 采不到手势；`flushSync` 再叠加整屏推送。A/B 全屏指针无渐变弧，故正常。

**修复**：
1. Infograph **色轨/读数/子盘缓存**（PSRAM）：数据不变时秒针只 blit 缓存再叠针
2. `igArcGrad` 段数 capped（约 ≤28，原可到 120）
3. 手指按下 / 拖页 / `sliding` 时**跳过** Infograph 重绘与 `faceDirty`/`wxDirty` 整页重建
4. 换 type8 / 离开 C / `#BGD` 时 `invalidateInfographCache()`

## 建议实机验证

1. 中央=全屏指针 + C Infograph：秒针走动，可左/右/上/下划切页，不卡死
2. A/B 全屏指针回归：划页仍正常
3. 切出 Infograph 仍清 type8 底图（FO-203）
