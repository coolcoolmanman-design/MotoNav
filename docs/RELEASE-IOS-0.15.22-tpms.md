# MotoNav iOS 0.15.22-tpms

通用圆屏固件配套 · 基于 `0.15.21-tpms` 叠 **连接动画屏向** 与 **时速圆标间距** 修正。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

- **连接动画屏面**：`ConnectPuckSceneView` 屏贴绕 Z 轴转 180°（保留 UIKit→SceneKit Y 翻转），导航图与立式设备同向（不再上下颠倒）。
- **圆屏卡设备图**：`DevicePuckHero` / `ConnectPuckScreen` 重导出 mockup C；时速白圈内 `28` 与 `km/h` 间距收紧至约 8–16px 净空（无重叠、无椭圆各向异性压扁）。
- **我的 → 关于**：副标题保持「信息和调试」。
- 继承 0.15.21 / 0.15.20：mockup C 导航内容、轨迹「骑行」→「导航」、地图水印剥离、二级 `NavigationLink` 侧滑返回、卡片去灰描边、冷淡科技色板。

## 固件

- 请同时刷入现网配套固件 Draft（`7.12.11-opt.8` 或更新）。本工作流附带的 panel0 zip 可能仍为分支内源码构建基线。本版不做固件改动。

## 安装

- IPA：`MotoNav-0.15.22-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
- 产品图为公开实拍合成，无营销叠加文案、无私人路径。
