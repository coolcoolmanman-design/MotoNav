# iOS 0.16.2.60

配套固件 `7.12.10-ui.2-zelda40.9-musicA.63`。基线 0.16.2.59 / musicA.62
（`5a96df1`）。同包落地 **FO-208**（arcs-v6 切线）+ **FO-209**（Infograph 触摸卡死修）。

- iOS Draft：https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-ffde7765b10d86fd84dd
- FW Draft：https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-5e1d48a61ac4cca2ef31
- tip：`aff3331`

## 变更

### FO-208 · Infograph arcs-v6
- 烘焙底图 / 预览换成 arcs-v6（切线平行；BAT/UVI 字脚朝外；天气/AQI 字脚朝圆心）
- 固件弧读数改为切线字形 blit（与确认效果图一致）
- **升级后须重新下发 Infograph 底图**

### FO-209 · 触摸卡死（随 musicA.63 同包）
- chrome 缓存 + 触摸期间跳过 Infograph 重绘（自 musicA.62）

## 建议实机验证

1. 升级 → C Infograph → 重新下发底图 → 弧字朝向与预览一致
2. Infograph 页可划页，不卡死
