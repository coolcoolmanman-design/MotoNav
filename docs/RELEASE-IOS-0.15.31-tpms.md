# MotoNav iOS 0.15.31-tpms

通用圆屏固件配套 · 基于 `0.15.30-tpms`：配对 **opt.10 + 0.15.30** 后 **导航线已恢复**，但 **底图分块空白仍在**。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### P0 · 底图空洞再收紧（保留 type-2 优先）
- `emptyMaskSoftenKmh`：**55 → 40**
- 旧密瓦覆盖判定 margin **50 → 200**（勉强盖住也算不安全）
- bypass：新块盖住且更近中心（有任意材料）；或新块盖住且 materials ≥ min/4
- `needsRefresh`：≥40 且距瓦片中心 &gt; 700 m 也刷新
- **保留** 0.15.30 type-2 插队 / 路线重发；RLE@40；CAP,MD；type5

### 固件

请继续配对 **`7.12.11-opt.10`**。

## 安装

- IPA：`MotoNav-0.15.31-tpms-unsigned.ipa`
