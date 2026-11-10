# iOS 0.16.2.61

配套固件 `7.12.10-ui.2-zelda40.9-musicA.66`。基线 0.16.2.60 / musicA.65。
同包落地 **FO-212**（UVI 弧接真值）。安卓端本版不改。

- FW Draft（musicA.65 基线稿）：https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-7c7d805db8f10e78b4eb
- iOS Draft（0.16.2.60 基线稿）：https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-ffde7765b10d86fd84dd

## 变更

### FO-212 · UVI 弧接真值（和风天气）
- 天气请求新增第 6 路 `/weather/v1/daily?days=1`，解析 `days[0].uvIndexMax`（当日峰值 0~12，失败回落 -1）。
- `#WX` 报文在 rain bars 末尾追加 `,uvi` 字段；既有字段位置不变，固件可向后兼容。
- 天气详情页新增一行「紫外线 N（今日峰值）」。
- 固件侧：UVI 弧中心读数由「--」改为真值，满轨色标保持 arcs-v6 不变；缓存签名纳入 UVI。

## 数据实测说明
- 空气接口 `indexes` 只返回 `cn-mee`（AQI），无 UVI。
- `/indices/v1/daily` 当前 key 返回 404（无生活指数权限）。
- 故 UVI 取每日预报的 `uvIndexMax`，为**当日峰值而非实时值**。

## 建议实机验证

1. 升 FW musicA.66 + IPA 0.16.2.61 → C Infograph，等天气刷新（缓存 10 分钟，可下拉强制刷新）。
2. UVI 弧中心显示数字（如 4）而非「--」；天气详情页有「紫外线」一行。
3. 无网络 / daily 请求失败时 UVI 回落「--」，不影响其余天气读数。

## 构建修正
- CI 分支白名单加入 `cursor/uvi-weather-wx`（固件 + iOS 两个 workflow）。
