# iOS 0.16.2.120

- **FO-285 首页顶卡收紧（效果图落地）：**
  - 去掉「今天，骑向哪里？」下副文案
  - 右侧圆屏设备约 46% 原直径，完全落在卡圆角内（无溢出）
  - 标题相对缩小设备垂直居中；MOTONAV 仍在标题上方
  - 定位/搜索/粘贴/规划/footer 整体上移，收紧顶卡留白
  - 设备图仍为圆屏 Tab 同款 soft `DevicePuckHero`（device-nav-c-hero-820）
- **保留 0.16.2.119：** Option A「换一块圆屏」+「选择圆屏」sheet；鸟瞰回归相关 iOS 行为
- 基线：`cursor/birdseye-regression-fix-9c25` @ 0.16.2.119 / musicA.136
- 配对固件（本包未改固件）：鸟瞰线可用 **musicA.137**；否则沿用 **musicA.136**

# retrigger: FO-285 home hero tight / 0.16.2.120
