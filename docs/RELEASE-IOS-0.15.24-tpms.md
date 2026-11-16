# MotoNav iOS 0.15.24-tpms

通用圆屏固件配套 · 基于 `0.15.23-tpms` 修正 **连接弹层圆屏预览正立**（mockup C）。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### 连接动画（P0）
- 根因：0.15.23 去掉 `screenNode.eulerAngles.z = π` 后，仍保留 UIKit→SceneKit 的 Y `contentsTransform`（`scale(1,-1)` + `translate(0,1)`），净效果仍是整幅导航图 180° 倒立。
- 修复：同时去掉该 Y `contentsTransform`；屏节点不设 Z=π。`ConnectPuckScreen`（mockup C）本身已正立，SceneKit 对 `UIImage` 按正立采样——**零额外校正**才是正确净取向（上 turn+200 m，下 28 km/h，文字可读）。
- 不旋转单个标签；整屏贴图/平面一次对齐。

### 继承 0.15.23（未改）
- 圆屏底图毛边/花纹分类与 `despeckleRoads`。
- `DevicePuckHero` 椭圆透视烘焙（mockup C）；关于副标题「信息和调试」。

## 固件

请同时刷入 **`7.12.11-opt.9`**。勿与 stopwatch / flag 协议包交叉。

## 安装

- IPA：`MotoNav-0.15.24-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
