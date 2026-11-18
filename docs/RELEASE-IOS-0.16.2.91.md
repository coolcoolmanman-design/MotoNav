# iOS 0.16.2.91 · musicA.100

加长前台沿线预取 + 回前台建筑补采；配套申花中央干净动画 soft_keep bake。Draft-only。

## 本版变更

- **预取加长**（活跃 App）：
  - 沿线显示前瞻 **800 → 1600 m**
  - 预取时间地平线 **12 → 24 s**
  - 近距节点窗 **target+2 → +5**（约 +1.8 km）
  - 跟车前移 **450 → 750 m**
- **回前台补采**：`setRenderingAvailable(true)` 清退避、`resumeForegroundCatchUp` 优先补缺失建筑包，并尝试立刻叠轮廓。
- **锁屏策略不变**：非前台不新开高德双截图；可继续发已缓存包。
- **SoftAP 仍关闭**。
- 配套固件 **musicA.100**：申花 idle/conn 用 `user-ref-100pct-v4-clean-sized` soft_keep→harden 重 bake（无透底；体育场仍为 dynBg style 8）。

## 版本

- iOS `0.16.2.91` · build `100`
- FW `7.12.10-ui.2-zelda40.9-musicA.100`

> CI note: maps unit test updated for 24s prep horizon.
