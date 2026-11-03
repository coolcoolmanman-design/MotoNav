# MotoNav iOS 0.16.2.10-tpms

Draft-only · 固件不变（配对既有 `7.12.10-ui.2-zelda40.9-musicA.7` 可换表盘）。无 PR。

基线：合并 `0.16.2.9-tpms` 表盘选择 tip + `0.16.2.8-tpms` 行程/连接/底栏玻璃。

## 本包

1. **合并** `0.16.2.8` 行程 Tab MVP、连接 HUD 盖板修复、普通 frosted 底栏；保留 `0.16.2.9` 待机表盘选择 UI
2. **按压反馈**：Tab / GlassChip / GlassButton / 行程主交互 — 缩放高亮 + 触感
3. **连接动画地面影**：空心环/近透明贴图 → 程序化软椭圆落地投影（保留磨砂银 3D 壳）
4. **行程补齐**
   - 城快 vs 高速（GPS 速度启发式；不足则诚实空态）
   - 高清 JPEG 海报导出 + 微信友好分享（文件 URL / LSApplicationQueriesSchemes）
   - 单日明细下钻页
   - 年度足迹专题图
   - 总览副标题固定「行程概述」
5. **版本** `MARKETING_VERSION 0.16.2.10` · `CURRENT_PROJECT_VERSION 25` · `APP_VERSION iOS v0.16.2.10-tpms`

## 安装

同 Bundle ID 覆盖安装；固件若需表盘功能请刷 musicA.7。
