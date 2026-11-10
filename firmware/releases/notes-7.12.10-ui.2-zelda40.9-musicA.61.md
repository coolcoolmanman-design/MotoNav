# 固件 7.12.10-ui.2-zelda40.9-musicA.61

基线 musicA.60 / iOS 0.16.2.58（`e6f9848` · Draft
`untagged-fe6221ee417e9e019273`）。配套 iOS **0.16.2.59**。

## FO-206 · Infograph arcs-v5

- 叠层对齐确认稿：色轨加厚嵌灰轨；AQI/UVI/天气渐变满轨；电量蓝弧按 %
- 弧中读数屏幕正向叠色轨中央；两端温贴端帽；无 TEMP；无中心电量 pill
- 烘焙底图须随 App 重下发（标签 AQI/UVI/BAT 在壁纸左端）

## FO-203 · 切出清盘

`#FC3` 离开 Infograph（中央≠全屏指针或 ab≠C）时清 type8 自定义壁纸
（`bgCustomOn` / Flash），避免烘焙表盘残影叠到动画/时间。

## FO-207 · 音乐样式 5

`#MS,0..5`：新增「全屏转动封面」——满圆慢旋封面 + 细进度 + 底部半透胶囊播控。

## 建议实机验证

- C Infograph + 新底图：弧中正向、电量弧按 %、无中心 pill
- Infograph → 动画/时间：无表盘残影
- 音乐样式「全屏转动封面」播放中慢旋、胶囊可点
