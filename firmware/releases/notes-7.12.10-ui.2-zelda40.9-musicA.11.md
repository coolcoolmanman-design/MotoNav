# MotoNav 通用圆屏固件 7.12.10-ui.2-zelda40.9-musicA.11

在 musicA.10（sticky 地图 + 近空 NACK + 纯圆封面唱针）基础上：

## 修复：亮屏建筑轮廓顶部「平切」

- **根因（固件）**：musicA.10 sticky 上一张方瓦时，位姿驶近瓦片北边，1200² 栅格的**直线北界**投影成圆屏顶部水平截断；建筑网格被齐腰切开，截线上部易呈黑弧。
- **非**「导航久了轮廓淡出」那条 iOS roof-budget 链路（见 `building-outline-fade-upstream-gap.md`）。
- **修复**：`rasterCoversView` 门闸——错帧 sticky 仅当视口仍被瓦片覆盖；否则退出地图模式等新瓦。`drawMapFrame` 先 `fillScreen(C_BLOCK)` 再画栅格，去掉未写像素黑弧。保留 musicA.10 帧对齐 sticky 防箭头回退。

## 音乐唱针修订（对齐更新 mock）

- 臂长缩短（`TONEARM_LEN ≈ 0.58R`）
- 矩形头壳（cartridge headshell）
- **黑色**针尖
- 纯圆封面 / 氛围 / 方案 A / 玻璃三键不变

## 配套

- Firmware Draft only；IPA 仍 `0.16.2.12`
- 基线：musicA.10 `25aa431` / `untagged-7239843c35fa4a9cee9d`

## 升级方式

App 镜像烧录到 0x10000，整包烧录到 0x0。校验 SHA256SUMS 后再刷。
