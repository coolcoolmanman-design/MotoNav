# iOS 0.16.2.59

配套固件 `7.12.10-ui.2-zelda40.9-musicA.61`。基线 0.16.2.58 / musicA.60
（`e6f9848` · Draft iOS `untagged-2cac71cc224b15ba8ba3` /
FW `untagged-fe6221ee417e9e019273`）。

## 变更

### FO-206 · Infograph arcs-v5
- 烘焙底图 / 预览换成 arcs-v5（撑满；AQI/UVI/BAT 标签左端；无 TEMP；无中心电量 pill）
- 固件色轨：AQI/UVI/天气渐变满轨，电量蓝弧按 %；弧中 `温/AQI/UVI/BAT%` 屏幕正向叠色轨；两端温贴端帽
- **升级后须在 App 重新下发 Infograph 底图**（选 C 或点「重新下发底图」）

### FO-203 · 切出 Infograph 清盘
- 离开 C Infograph / 切出全屏指针时清 type8 烘焙底图，禁止残影叠动画/时间
- iOS `clearInfographBaseIfActive` + 固件 `#FC3` 离开态清壁纸双保险

### FO-204 · 外观
- 「我的」外观：自动（跟随系统白夜）/ 手动白 / 手动黑

### FO-207 · 音乐全屏转动封面 B
- 播放风格新增「全屏转动封面」：满圆慢旋封面 + 细进度 + 底部半透胶囊播控（`#MS,5`）

## 建议实机验证

1. 升级后切 C Infograph → **重新下发底图** → 弧中正向读数、无中心 pill
2. Infograph → 中央动画/时间：表盘底图消失，无残影
3. 我的 → 外观三段切换生效
4. 圆屏 → 音乐控制 → 全屏转动封面：慢旋 + 胶囊三键
