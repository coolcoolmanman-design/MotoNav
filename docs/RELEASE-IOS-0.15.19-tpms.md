# MotoNav iOS 0.15.19-tpms

通用圆屏固件配套 · 基于 `0.15.18-tpms` 叠 **轨迹缩略图标 + 圆屏产品图/胎压温电** 实测修正。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

- **轨迹**：列表 / 大卡缩略图起终点改为小圆点标注（不再用偏大的 MapKit Marker 图钉）；详情图略大圆点。
- **圆屏卡**：3/4 角产品实拍合成透明 PNG（银壳保留；屏内为 HD 导航地图效果，去掉天气/笑脸）；白棚背景已抠除。
- **胎压监测**：每轮明确展示 **压力（bar）+ 温度（°C）+ 电量（%）**（`Reading.kpa / tempC / battPct`）。
- 冷淡科技色板与 0.15.18 连接动画 / Tab IA 不变。

## 固件

- 请同时刷入现网配套固件 Draft（`7.12.11-opt.4` 或更新）。本工作流附带的 panel0 zip 可能仍为分支内源码构建基线。

## 安装

- IPA：`MotoNav-0.15.19-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
- 产品图为公开实拍合成，无营销叠加文案。
