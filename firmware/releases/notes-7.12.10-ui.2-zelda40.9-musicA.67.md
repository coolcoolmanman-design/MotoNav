# 固件 7.12.10-ui.2-zelda40.9-musicA.67

基线 musicA.66 / iOS 0.16.2.61（`de5be595`，Draft `untagged-ed4a14cefc230e1de9c6`）。**固件 + iOS 双端同包**（FO-213），配套 IPA **0.16.2.62**。
安卓端本版不改。

## FO-213 · Infograph 实机三修（灰轨 / 幽灵白针 / 弧字切线）

对 musicA.66 实机反馈：

1. **白盘外缘多余灰轨** — BASE 烘焙灰轨与固件彩弧灰边双层叠加。  
   - BASE 换 **arcs-v7**（去掉灰轨与 AQI/BAT/UVI 弧标签）。  
   - 固件 `igDrawUnderHands` 先画同半径灰轨再嵌彩弧；标签改 FW 切线绘制。
2. **幽灵白针** — Infograph 浅底上仍用 `csPal().ink`（待机深色白墨）。  
   - `igDrawHands` 时/分针与针头固定黑墨 `0x0000`。
3. **弧字非切线** — 旧 `th=-a` 旋转把字形摆成近径向。  
   - `igBlitGlyphTangent` 改显式切线/径向基向量；描边保留；字脚朝向仍按 FO-208（天气/AQI 朝内，BAT/UVI 朝外）。

保留 FO-208 / FO-209 / FO-210 / FO-211 / FO-212。

## 建议实机验证

1. **必须同升** FW musicA.67 + IPA 0.16.2.62（新 BASE 与 FW 灰轨配套；混升会缺轨或双轨）。
2. C Infograph：四弧灰轨嵌合彩弧，白盘外缘无双层灰线。
3. 时/分针为黑色，无白影叠针。
4. 弧上数字与 AQI/BAT/UVI 标签沿弧切线，字脚方向与 arcs-v6 确认稿一致。
5. UVI 真值、左 WX 符号、划页不卡死保持不变。
