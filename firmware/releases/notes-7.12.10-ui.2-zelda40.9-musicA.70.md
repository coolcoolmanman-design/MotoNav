# 固件 7.12.10-ui.2-zelda40.9-musicA.70

基线 musicA.69 / iOS 0.16.2.63。**固件 + iOS 双端同包**（FO-216），配套 IPA **0.16.2.64**。
安卓端本版不改。两份 release 保持 draft，不 publish。

## FO-216 · Infograph C 实机反馈六项修正

按确认稿 `infograph-fix-mockup.html` 落地：

1. **① 白盘外缘灰弧整圈移除**：FW `igDrawUnderHands()` 删 4 段 `hwGrey` 灰轨，App 侧 `07-infograph-base.png` 重烘焙（不再带灰弧）；彩弧直接贴黑底，白盘边缘只留 60 格细刻度。
2. **② 只留一套动态针**：BASE 里烘焙的静态时/分/秒针全部删除（原来与 FW 实时针形成重影）。
3. **③ 顶部电量 pill 删除**：`drawBattery(CX-W/2, 68, false)` 及 `slotMask` 门卫整块移除；电量仍由 BAT 弧与读数承担。
4. **④ 字标重定位 + 清残影**：`MotoNav` 按确认稿落位（baseline y≈64），BASE 上方烘焙残留的旧白字清掉；白盘上缘仅剩约 6px 余量，不再继续上移以免字顶切进黑底。
5. **⑤ 时/分针双层灰描边**：浅灰 `#a8aeb3`（RGB565 `0xAD76`）+ 深灰 `#6b6d6d`（`0x6B6D`）+ 黑芯；时针 15→12→10、分针 12→9→7，剑尖与轴心同步外扩；红秒针不加描边。
6. **⑥ 针头由方变尖**：`thickLine()` 的 butt 平头端面原来一直画到针尖顶点，把剑形三角的尖削平。新增 `igLayeredHand()`，粗线终点回退 19px（剑尖底边在 20px 处），端面被三角底边完全盖住；针长不变、剑形肩部保留。

## 配套与刷新

- **必须同升** FW musicA.70 + IPA 0.16.2.64，并在 App 里重新下发一次 type8 底图（`InfographDialBase`），否则仍是 69 的旧 BASE。
- ⑤⑥ 只改固件；①②③④ 需 BASE 重烘焙并随 IPA 下发。App 设置页的 Infograph 预览图同步换成确认稿 AFTER。

## 建议实机验证

1. 选 C Infograph：白盘外缘无灰弧、无第二套静态针、顶部无绿色电量块、`MotoNav` 完整不被切顶。
2. 时/分针为浅灰+深灰双层描边、针尖是真尖；红秒针无灰边。
3. 四子盘（上日期 / 下 LIVE G / 左 F / 右 R）与彩弧读数正常。
