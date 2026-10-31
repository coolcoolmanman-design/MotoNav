# MotoNav iOS 0.15.29-tpms

通用圆屏固件配套 · 基于 `0.15.28-tpms`：配对 **opt.10 + 0.15.28** 后 ≥60 圆屏分块空白仍复现 → 再收紧传输与空掩码。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### P0 · 空掩码软化 + 更早 plain RLE
- 现场：固件 **opt.10** + IPA **0.15.28** 仍空白。
- `dxrMaxKmh`：**55 → 40**（≥40 跳过 DFL/DXR，只发 plain RLE）。
- `dxrDisableKmh`：**40 → 25**（25–40 仅 DFL）。
- `emptyMaskSoftenKmh = 55`：密瓦已在屏上时，若新包材料不足，但新块盖住车姿而旧块已不盖（或两者都不盖且新中心更近）→ **bypass** empty-mask，避免奶油分块空洞。事件：`packet-empty-mask-bypass`。
- 分类 despeckle：材料 &lt; 2× empty 地板时跳过，避免薄路被洗空。
- `needsRefresh`：≥55 提前量 16s；≥40 为 12s。
- 保留：CAP,MD / DFL / DXR（低速）/ type5；无 FLAG/type6。

### 固件

请继续配对 **`7.12.11-opt.10`**（本版不改固件）。

## 安装

- IPA：`MotoNav-0.15.29-tpms-unsigned.ipa`（unsigned 侧载）。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
