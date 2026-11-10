# 固件 7.12.10-ui.2-zelda40.9-musicA.68

基线 musicA.67 / iOS 0.16.2.62（FO-213）。**固件小 Draft**（FO-214 Infograph 四子盘重排）；IPA 仍用 **0.16.2.62**（BASE arcs-v7 不变）。
安卓端本版不改。

## FO-214 · Infograph 四子盘：日期 / F·R 胎压 / LIVE G

确认稿 `07-landing-preview-top-date.png`：

| 方位 | 内容 |
|---|---|
| 上（N） | 星期缩写 + 日（如 `TUE` / `10`，日不补零） |
| 左（W） | 前胎压 `F` + `tpFrontKpa` |
| 右（E） | 后胎压 `R` + `tpRearKpa` |
| 下（S） | `LIVE G` + `liveG()` |

- 天气不再占子盘（气温/AQI/UVI 仍在外弧）。
- `igDataSig` 纳入日期/星期与后胎压，跨天与 #TP 更新会刷新缓存。
- 保留 FO-213：黑针、切线弧字、BASE arcs-v7 + FW 灰轨/彩弧配套。

## 建议实机验证

1. 升 FW musicA.68（IPA 仍 0.16.2.62）→ C Infograph。
2. 上子盘为当日星期+日；下为 LIVE G；左右为 F/R 胎压（无 #TP 时 `--`）。
3. 白盘外缘无双层灰；黑针无白影；划页不卡死。
