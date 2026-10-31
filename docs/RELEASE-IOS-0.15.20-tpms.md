# MotoNav iOS 0.15.20-tpms

通用圆屏固件配套 · 基于 `0.15.19-tpms` 叠 **轨迹文案 / 地图水印 / 圆屏导航图 / 二级转场 / 去灰描边** 实测修正。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

- **轨迹 Tab**：用户可见「骑行」文案统一改为「导航」（标题、空态、列表、详情、摘要等）。
- **地图缩略**：MapKit 缩略 / 详情尽量隐藏 Apple / 高德 attribution 字样（负 layoutMargins + 裁切放大 + 子视图剥离）。若系统仍强制极细法律行，属 OS 限制。
- **圆屏卡**：新 HD 导航屏图等比合成到银壳 bezel（`DevicePuckHero`），无拉伸、无天气/笑脸残留。
- **二级菜单**：圆屏显示 / 圆屏设置、我的三级入口、轨迹详情改回 `NavigationLink` 系统 push，支持侧滑返回与短转场。
- **选项卡去描边**：卡片 / 列表行 / chip / 分段选项卡去掉灰色描边，靠填充与间距分层；告警态保留色描边。
- 冷淡科技色板不变。CoverBridge 灰度编码未改（固件 opt.6 接收端已转灰；opt.7 封面策略待定，不阻塞本 IPA）。

## 固件

- 请同时刷入现网配套固件 Draft（`7.12.11-opt.6` 或更新）。本工作流附带的 panel0 zip 可能仍为分支内源码构建基线。

## 安装

- IPA：`MotoNav-0.15.20-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
- 产品图为公开实拍合成，无营销叠加文案。
