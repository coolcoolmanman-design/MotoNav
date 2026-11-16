# iOS 0.16.2.72

基线 Draft `ios-v0.16.2.71` / tip `1d31788`（musicA.79）。本版 **仅 App**：Duo 外/内屏显示逻辑 + TPMS 扫描速度与 UI。
固件仍用配套 `7.12.10-ui.2-zelda40.9-musicA.79`（**无需新刷**，高温阈值改为 App 默认对齐固件 60°C）。
Release 保持 Draft，不 publish。安卓端本版不改。

## 变更

### Duo 外屏 / 内屏

- 合盖：继续右竖轨全量四 Tab（导航/行程/圆屏/我的）贴信号/电量侧；禁底栏 / 左轨主 chrome。
- 合盖内容 Cover 精简：导航单主 CTA「规划路线」；行程/我的可进概述/常用项。
- 内屏 Path A：`sidebarAdaptable` 贴信号/电量同侧。
- 宽屏导航辅栏：去掉「行程」「全部收藏」Tab chip，保留连接摘要 + 收藏。
- 宽屏行程：同 Tab 左右分栏（概述 | 日明细）。
- 宽屏圆屏：左 b7 + TPMS · 右设置入口列表。
- Connect 叠层保留，本轮不抛光。

### TPMS（App-only）

- P0：前台 / 未双绑 / repairing → 立刻无过滤 BLE 扫描（不再等 ~60s UUID 过滤空心跳）。
- P0/P1：未双绑或 repairing 时 `allowDuplicates`；双绑后收紧。
- 空态文案改为「等待气嘴广播」+ 非 GATT 说明条；LIVE / 配对状态行对齐 mock。
- ± 阈值热区 `contentShape` ≥44（视觉仍 28）。
- 删除 `ContentView.tpmsCard` 双实现；测试帧仅「我的 → 关于 → 调试」可见。
- 默认 `highTemp` **60°C**（对齐固件 `TpmsPage` 着色；已写入 UserDefaults 的旧值不变）。
- `BleLink.onLinkReady`：有传感器读数时强制续发 `#TP`。
- 重新配对 sheet：三步指引 + 空列表 ADV 说明（低风险对齐 mock）。

## 版本

- `MARKETING_VERSION` → `0.16.2.72`
- `CURRENT_PROJECT_VERSION` → `81`

## 建议实机验证

1. Duo 合盖：右轨四 Tab；导航仅「规划路线」主 CTA；行程/我的可点入。
2. Duo 展开：导航无辅栏 chip；行程左右分栏；圆屏左设备+TPMS / 右设置入口。
3. TPMS：前台进圆屏 Tab 应立即无过滤扫描；插拔气嘴电池后空态→LIVE；重连圆屏后 `#TP` 立刻出现。
4. 阈值 ± 热区易点；生产面无「发送测试帧」。
