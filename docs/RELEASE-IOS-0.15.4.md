# MotoNav iOS 0.15.4 · Tab 条高德式双态彩色图标

基于 iOS 0.15.3 源码迭代。导航条 Tab 改为**高德式双态图标**：选中项 = 实心填充图标 + 功能专属彩色文字，未选中项 = 灰色线性图标（无色透明），一眼区分功能归属。继续沿用 0.15.3 的无描边软胶囊 + 阴影浮起选中底，玻璃导航条本身不变。本轮由 AI 开发；随发布流水线自动打包未签名 IPA。

## 变更内容

| 项目 | 0.15.4 的实现 |
|---|---|
| 图标双态 | 未选中线性（`magnifyingglass` / `map` / `speaker.wave.2` / `gearshape` / `key`），选中实心填充（`magnifyingglass.circle.fill` / `map.fill` / `speaker.wave.2.fill` / `gearshape.fill` / `key.fill`） |
| 功能配色 | 首页搜索 = 品牌粉 `#E5219F`，圆屏显示 = 青 `#0E97AC`，导航语音 = 橙 `#F08A00`，圆屏设置 = 紫 `#AF52DE`，地图 Key = 绿 `#34A853` |
| 深色模式 | 每个 Tab 色单独提亮（粉 `#FF6FC4` / 青 `#62E9F5` / 橙 `#FFB340` / 紫 `#C873F0` / 绿 `#4CD47C`），玻璃上"发光"保持可读 |
| 覆盖范围 | 底部横条（普通机型 / Duo 展开竖屏两栏）与右侧竖栏（Duo 外屏 / 横屏）共用同一套 `TabSpec` + `AppTheme.tabAccents` |

## 版本与流水线

- 版本号同步：`RELEASE_VERSION`、`APP_VERSION`、`MARKETING_VERSION` 均为 0.15.4，Share 扩展跟随 `$(MARKETING_VERSION)`。
- 发布流水线 `release-ios.yml` 触发分支加入 `ios-v0.15.4`，push 自动打包未签名 IPA 并发布 Release `ios-v0.15.4`。
- 设计稿同步更新：`docs/ui-0.15.3/液态玻璃UI效果图.html`（Tab 条规格卡片改为实心彩色图标示意）。

## 验证边界

- CI 内完成：到达生命周期 / BLE 队列 / 地图栅格 Swift 回归测试 + iPhone arm64 Release 编译 + 未签名 IPA 打包。
- 未做：真机视觉验收（Tab 配色在实机玻璃材质上的观感需自行确认）、旧系统（iOS 17/18）降级毛玻璃下的配色对比度仅按 token 推算。
- 源码不含 Apple 证书、描述文件、私人签名或地图 / 天气 Key；Key 均为运行时在 App 内填写。
