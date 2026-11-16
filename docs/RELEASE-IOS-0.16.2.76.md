# iOS 0.16.2.76 · musicA.82

基线 **0.16.2.75** / **musicA.81**（`48a3c9d`）。Draft 不 publish。

## 本包

**申花开机锯齿修复（David 实机反馈）**

- 根因：musicA.80 半分辨 233² + 最近邻 2× 铺满 → 队徽外缘硬方锯齿。
- 修法：全分辨 466² zlib + 软边 alpha 烘焙；`renderShenhua` 与 GTA 一样 1:1 直写 FB。
- 源帧：`media/shenhua-anim-mockup-v2/boot/`；idle/connected / 等红灯未改。

## 版本

- iOS `0.16.2.76` · build `85`
- FW `7.12.10-ui.2-zelda40.9-musicA.82`
