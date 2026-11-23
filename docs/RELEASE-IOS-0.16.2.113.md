# MotoNav iOS 0.16.2.113

## 修复（相对 0.16.2.112）

- **首页 Hero 文案被裁：** `.frame(height:)` 默认竖向居中，把 `topLeading` 的 `MOTONAV` / 「今天，骑向哪里？」裁出可视区（只剩「…出发。」）。改为 `alignment: .top`，文案独占左栏。
- **设备硬切银边：** 新增 `DevicePuckHeroSoft`（外圈银边溶透明 + 左缘融入）；Hero 用 Soft 资源，右偏溢出 + 左/底 mask + 页色 wash，圆盘溶进页面背景，不再硬贴灰底。

## 保留

- 深色「粘贴高德分享的路线」白字实心蓝（0.16.2.112）。

## 配套

- 建议固件：`v7.12.10-ui.2-zelda40.9-musicA.131`（本轮仅 iOS）
