# iOS 0.16.2.122

- **FO-287 soft device + 换屏历史（效果图落地）：**
  - Soft device：`DevicePuckHeroSoft`（device-nav-c-hero-820 DNA）**保宽高比**溶进卡面；下缘止于搜索行顶；边缘 soft fade；**非**横拉填卡 / 低清 wallpaper（修正 0.16.2.121）
  - **MOTONAV** 加大作品牌标题；无副文案；卡高保持 tight ~898（与 0.16.2.120/121 同）
  - 「换一块圆屏」Option A 保留；sheet「选择圆屏」分 **历史设备** + **附近发现**
  - 历史持久化（`ble_screen_history`）；点按历史 → 断开当前再连所选（热切换，无需杀进程）
- 基线：`cursor/home-hero-tight-23f9` @ 0.16.2.121
- 配对固件（本包未改固件）：鸟瞰线可用 **musicA.137**；否则沿用 **musicA.136**

# retrigger: FO-287 soft device + history switcher / 0.16.2.122
