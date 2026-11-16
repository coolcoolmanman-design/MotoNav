# iOS 0.16.2.64

配套固件 `7.12.10-ui.2-zelda40.9-musicA.70`。基线 0.16.2.63 / musicA.69。
同包落地 **FO-216**（Infograph C 实机反馈六项修正）。安卓端本版不改。
两份 release 保持 draft，不 publish。

## 变更

### FO-216 · InfographDialBase 重烘焙
- `07-infograph-base.png`（type8 底图）按确认稿重绘静态层：黑底 + 白盘 r=185 + 60 格细刻度 + 四个空子盘环 + `MotoNav` 字标。
- 移除 69 BASE 里烘焙的灰弧、一整套静态时/分/秒针、旧白字残影；字标按确认稿落位。
- 彩弧 / 弧标签 / 子盘读数 / 指针仍由固件实时绘制，底图不再重复烘焙。
- `07-landing-preview-standby.png`（App 内 Infograph 预览）同步换成确认稿 AFTER 渲染，避免设置页仍显示旧灰弧、双套针与电量块。

## 配合固件

- `igDrawUnderHands()` 删灰轨与顶部电量块；`igDrawHands()` 改双层灰描边 + 尖针（`igLayeredHand()`）。
- **必须同升 FW musicA.70 并重新下发一次 type8 底图**，否则设备仍用 69 的旧 BASE。

## 建议实机验证

1. 升 FW musicA.70 + IPA 0.16.2.64 → C Infograph，确认底图重下发后白盘外缘干净、无双套针、无顶部电量块。
2. 时/分针双层灰描边、针尖收成尖；字标完整。
