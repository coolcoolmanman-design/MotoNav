# MotoNav iOS 0.15.28-tpms

通用圆屏固件配套 · 基于 `0.15.27-tpms`：**≥60 km/h 圆屏底图分块空白**（opt.9 + 0.15.27 配对仍复现）修复。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### P0 · 中高速底图分块空白
- 根因：plain-RLE / 关 DXR 门槛仍为 **80 km/h**；用户在 **≥60** 仍走 DFL/DXR。大位移 DXR NACK→重试烧掉覆盖窗口，再叠加 empty-mask 拒空包 / 过重 despeckle，圆屏留下过期瓦片外的奶油色分块空洞。
- `RasterMd.dxrMaxKmh`：**80 → 55**（≥55 直接 plain RLE，匹配 ≥60 实况）。
- `RasterMd.dxrDisableKmh`：**40**（40–55 只发 DFL，不开 DXR）。
- 中速 NACK：≥40 一次超时即 `mdLegacyOnly` plain RLE；map ACK 超时 1.2s。
- `classifyAmapBase` despeckle：**2 → 1** 遍，避免薄路被洗成 empty-mask。
- `MapCoveragePolicy.needsRefresh`：≥55 提前量 8s→12s，空掩码拒包时仍更早拉下一块密底图。
- 保留：CAP,MD / DFL / DXR（低速）/ empty-mask / type5；无 FLAG/type6。

### 继承 0.15.27
- 导航打开闪退门闩、自动连接动画、HUD 半透明、非机动车途经点。

## 固件

请同时刷入配套 **`7.12.11-opt.10`**（DXR 重叠收紧 + 轻量 despeckle，拒近空仍保留密瓦片）。勿与 stopwatch / flag 协议包交叉。

## 安装

- IPA：`MotoNav-0.15.28-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
