# MotoNav iOS 0.16.2.3-tpms

Draft-only · 配对固件 `7.12.10-ui.2-zelda40.9-musicA.2`。

基线：`0.16.2.2-tpms` 方案 A 冷雾银音乐页；本包修现场 bugs（封面马赛克/帧率 + 主页时钟/秒刻度）。

## 本包

1. **CoverPacket / CoverBridge → 240×240 CVR**（bulk type 5）  
   - 对齐固件 `COVER_PX=240`；设备端再双线性到 COVER_HI=320 定点数旋转  
   - 去满圆马赛克；固件侧 chrome 缓存旋帧提 FPS

2. **同仓固件 musicA.2**  
   - 主页时钟/秒环：塞尔达场景带后重画整环，滑回主页强制 `drawWaiting`  
   - Option A 色 / 无「MUSIC」标题 / 玻璃键不变

3. **版本** `MARKETING_VERSION 0.16.2.3` · `CURRENT_PROJECT_VERSION 19` · `APP_VERSION iOS v0.16.2.3-tpms`

## 刷机 / 安装顺序

1. **先刷**固件 Draft `zelda40.9-musicA.2`  
2. 再装本 IPA  
