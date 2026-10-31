# MotoNav iOS 0.15.30-tpms

通用圆屏固件配套 · 基于 `0.15.29-tpms`：配对 **opt.10 + 0.15.29** 后仍有分块空白，且 **主导航路线（type-2）消失**。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### P0 · 路线不被高速底图冲掉
- 根因：`BleBulkWriteQueue` 对 type-3 总插队、type-2 只追加；`checkResend` 在 `bulkPending` 时直接 return。0.15.29 的 RLE@40 + empty-mask bypass 让 type-3 持续占线 → 路线 ACK 超时永不触发 → 圆屏无紫线。
- type-2 **插到队首**；新 type-3 入队时 **保留等待中的 type-2 在地图之前**。
- `checkResend`：路线/幽灵重发 **不再** 被 `bulkPending` 挡住；≥40 km/h 路线超时 2s。
- 高速 `sendChunk` 前若路线未 ACK，先 `sendRoute()` 提升一次。
- 保留 0.15.29：empty-mask soften、plain-RLE@40、CAP,MD、type5。

### 固件

请继续配对 **`7.12.11-opt.10`**。

## 安装

- IPA：`MotoNav-0.15.30-tpms-unsigned.ipa`
- 高德 Key **不在包内**。
