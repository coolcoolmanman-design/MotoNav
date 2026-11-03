# MotoNav 通用圆屏固件 7.12.10-ui.2-zelda40.9-musicA.10

在 musicA.9（时间槽关闭修复 + 5 款表盘 + `#RC` 封面旋转/24fps）基础上：

## 修复：圆屏导航小地图不显示

- **根因 1**：`mapModeActive()` / `drawMapFrame` 要求 `rasterFrame == posFrame`。pose `#p` 抢跑 tile ACK 时立刻掉回大箭头，小地图开关已开也像「没地图」。
  现改为：有有效底图/矢量即保持地图模式；底图按绝对坐标绘制，允许沿用上一张瓦片。
- **根因 2**：musicA.7 从 musicA.5 重栽后丢失 opt.4 近空底图 NACK。近空 RLE 会装上纯 `C_BLOCK` 奶油底（路线还在）。
  现把 `rasterTooEmptyToReplace` 接回分步 `pollRasterWork` / `parseRaster`：拒空不回 `K,3`，保留上一张密瓦。

## 音乐页：纯圆封面 + 动态唱针

按定稿 mockup（无黑胶沟槽环）：

- 居中偏上纯圆裁封面慢旋（播放转 / 暂停停）；`COVER_HI=256`，旋帧仍最近邻。
- 封面取色氛围底；方案 A 文案色 + 玻璃三键；无顶栏「music」。
- 动态唱针：播放落在圆封面边缘，暂停抬离。

## 配套

- 固件 Draft only；IPA 仍用 `0.16.2.12`（协议未改）。
- 基线 tip：`untagged-4b62418a04573409da84` / firmware musicA.9 `8b55477`。

## 升级方式

App 镜像烧录到 0x10000，整包烧录到 0x0（保留 NVS 可跳过擦除）。校验 SHA256SUMS 后再刷。
