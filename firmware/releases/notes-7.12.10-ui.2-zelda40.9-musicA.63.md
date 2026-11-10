# 固件 7.12.10-ui.2-zelda40.9-musicA.63

基线 musicA.62 / iOS 0.16.2.59（`5a96df1`，FO-209 CI 已绿）。配套 iOS **0.16.2.60**。
同包：**FO-208** 切线弧字 + **FO-209** Infograph 触摸卡死修（不拆包）。

## FO-208 · Infograph arcs-v6 切线平行弧文字

- 弧中读数 / 天气两端温：切线平行（不再屏幕正向）
- **天气 / AQI**：字脚朝圆心
- **BAT / UVI**：字脚朝外（相对初版 v6 翻转，已确认）
- 烘焙 BASE / 预览换成 arcs-v6；标签在 BASE，色轨与数字由固件叠画
- **升级后须在 App 重新下发 Infograph 底图**

## FO-209 · Infograph 触摸卡死（自 musicA.62 保留）

- 色轨 / 读数 PSRAM 缓存；秒针只 blit 再叠针
- `igArcGrad` 段数 capped
- 触摸 / 拖页时跳过 Infograph 重绘

## 建议实机验证

1. 升 IPA + FW → C Infograph → **重新下发底图** → BAT/UVI 字脚朝外，天气/AQI 字脚朝圆心
2. 秒针走动，可划页切出 Infograph，不卡死
3. 离开 C 仍清 type8（FO-203）
