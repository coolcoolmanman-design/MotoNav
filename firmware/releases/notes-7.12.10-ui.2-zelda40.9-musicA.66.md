# 固件 7.12.10-ui.2-zelda40.9-musicA.66

基线 musicA.65 / iOS 0.16.2.60。**固件 + iOS 双端同包**（FO-212），配套 IPA **0.16.2.61**。
安卓端本版不改。

## FO-212 · Infograph UVI 弧接真值（和风天气）

- 数据源：和风 `/weather/v1/daily?days=1` 的 `days[0].uvIndexMax`（当日紫外线峰值，0~12）。
  空气接口 `indexes` 实测只有 `cn-mee` 无 UVI；`/indices/v1/daily` 该 key 无权限（404）。
- 协议 `#WX`：在 rain bars 之后**追加一个 uvi 字段**（不挪动既有字段）。
  老 App 不发该字段时固件回落 `-1`，UVI 弧显示「--」，向后兼容。
- 固件 `parseWeatherExtra` 读末尾 `values[5+wxRainN]` 存 `wxUVI`；
  `igDataSig()` 纳入 UVI 参与缓存签名，天气刷新后弧读数即时更新。
- UVI 弧**保持 arcs-v6 满轨色标不变**，只把中心读数从「--」换成真值（与 AQI 弧一致，不按值截断）。

## 保留 FO-208 / FO-209 / FO-210 / FO-211

## 建议实机验证

1. 升 FW musicA.66 + IPA 0.16.2.61 → C Infograph，等一次天气刷新（缓存 10 分钟，可下拉强制刷新）。
2. UVI 弧中心应显示数字（如 4）而非「--」；BAT / AQI / 天气读数保持不变。
3. 旧 App（0.16.2.60 及以前）连同一固件时 UVI 应回落「--」，不串值、不花屏。
4. 划页不卡死、灰轨嵌合彩弧、左 WX 为符号非温度。
