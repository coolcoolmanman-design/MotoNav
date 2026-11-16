# MotoNav iOS 0.15.3 · 全量液态玻璃 UI + iPhone Duo 规范适配

基于 iOS 0.15.2 源码改造。全部按钮改为**透明液态玻璃**，配色按玻璃场景重新调校，并按苹果 iPhone Duo 官方规范（iOS 27.1 Tech Talks / HIG「Designing for iPhone Duo」）完成折叠屏适配。本轮由 AI 开发；随发布流水线自动打包未签名 IPA。

## 液态玻璃设计系统

| 项目 | 0.15.3 的实现 |
| --- | --- |
| 玻璃按钮组件 | 新增 `GlassButton`（clear / tinted / prominent / destructive 四档）与 `GlassChip`（含 compact），iOS 26 使用原生 `glassEffect`（可交互、可淡染），旧系统自动降级 `ultraThinMaterial` 毛玻璃；按钮底色透明，强调色只做淡染 |
| 全量替换 | 首页搜索 / 连接 / 收藏 / 途经点 / 清除记录、开始导航 / 模拟导航 / 巡航、导航浮层芯片与退出胶囊、路线规划页、圆屏设置、Key 设置、转盘微调、天气与轨迹页——所有 `Button` 统一替换，无实色按钮残留 |
| 配色调校 | 罪恶都市粉青双强调色保留并按玻璃场景重调：浅色更深保对比（粉 `#E5219F` / 青 `#0E97AC`），深色提亮会发光（粉 `#FF6FC4` / 青 `#62E9F5`）；页面底色减紫压深 |
| 层次规则 | 玻璃只出现在控制层（按钮 / Tab 条 / 浮层），卡片与列表保持实色内容层，避免玻璃叠玻璃；一屏只保留一个 prominent 主操作 |
| 强制深色移除 | 路线规划 / 网页导航 / Key 设置页移除 `preferredColorScheme(.dark)`，跟随系统白夜自动切换 |

## iPhone Duo 折叠屏适配（苹果官方规范）

| 规范要求 | 0.15.3 的实现 |
| --- | --- |
| 内屏横屏 bars 竖排侧边 | `TabLayout.useSideRail`：内屏（regular 宽）横屏时导航条竖排右侧玻璃竖栏；竖屏回归底部水平横条；外屏合盖两种形态维持竖排右侧 |
| 内屏利用宽度做两栏布局 | 首页在展开态改为两栏：左栏巡航 / 路线规划 / 目的地，右栏圆屏实时状态 / 收藏 / 骑行轨迹；限宽 760 → 1080 |
| 避免引用 `UIScreen.main` | 新增 `TabLayout.sceneScreen`，从 foregroundActive windowScene 动态取屏幕，`UIScreen.main` 仅作旧系统兜底 |
| 外屏判定 | 保留两种外屏形态判定（窗口窄于物理面板 / 矮胖整屏），内屏横屏不再误判为外屏 |

## 其他

- 版本号同步：`RELEASE_VERSION`、`APP_VERSION`、`MARKETING_VERSION` 均为 0.15.3，Share 扩展跟随 `$(MARKETING_VERSION)`。
- 发布流水线 `release-ios.yml` 触发分支加入 `ios-v0.15.3`，从本分支 push 自动打包未签名 IPA 并发布 Release。
- 液态玻璃 UI 效果图与 iPhone Duo 适配样机见 `docs/` 及 Release 附件截图。
