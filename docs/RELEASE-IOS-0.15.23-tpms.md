# MotoNav iOS 0.15.23-tpms

通用圆屏固件配套 · 基于 `0.15.22-tpms` 叠 **圆屏底图毛边/花纹修复** + **连接动画正立** + **圆屏 Tab 设备图时速透视**。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### 圆屏底图（P0）
- 收紧灰路面分类：仅「比陆地更暗」且 `|Δlum|≥32`；软水色相先于灰路，避免河岸 AA 变成道路。
- `despeckleRoads` + 多数投票不弱推 land→road，去掉点状黑毛边/花纹；高速空掩码 / plain RLE / DXR 门槛保留。

### 连接动画
- 撤销 0.15.22 屏节点 `eulerAngles.z = π`。保留 UIKit→SceneKit Y 翻转。整幅导航预览正立（不再「m 002 / 82」）。

### 圆屏 Tab 设备图
- `DevicePuckHero` 重烘焙：屏幕内容（含 28 km/h）按机身椭圆透视 remap；保持 mockup C 转向条间距与时速 28/km/h 不重叠。`ConnectPuckScreen` 仍用平面圆图（由 SceneKit 平面投影）。

### 其他
- 我的 → 关于：副标题保持「信息和调试」。
- 冷淡科技色板不变。

## 固件

请同时刷入 **`7.12.11-opt.9`**（孤立道路 nibble 清除 + 近空 NACK）。勿与 stopwatch / flag 协议包交叉。

## 安装

- IPA：`MotoNav-0.15.23-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
