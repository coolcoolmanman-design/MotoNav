# iOS 0.16.2.62

配套固件 `7.12.10-ui.2-zelda40.9-musicA.67`。基线 0.16.2.61 / musicA.66（`de5be595` · Draft `untagged-ed4a14cefc230e1de9c6`）。
同包落地 **FO-213**（Infograph 灰轨/白针/切线三修）。安卓端本版不改。

## 变更

### FO-213 · Infograph BASE arcs-v7 + 固件三修
- `InfographDialBase` 换成 arcs-v7：去掉烘焙灰轨与 AQI/BAT/UVI 弧标签（改由固件绘制，消除双层灰）。
- 固件侧：灰轨+标签+彩弧同层；切线基向量弧字；时/分针黑墨。
- 必须与 musicA.67 同升；旧 IPA 底图仍含灰轨，配新固件会再次双层。

## 建议实机验证

1. 升 FW musicA.67 + IPA 0.16.2.62 → 选 C Infograph，确认 type8 底图重新下发。
2. 白盘外缘无多余灰线；黑针无白影；弧字切线可读。
3. UVI / WX 符号 / 划页行为与 0.16.2.61 一致。
