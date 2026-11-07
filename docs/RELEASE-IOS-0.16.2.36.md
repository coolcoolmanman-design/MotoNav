# MotoNav iOS 0.16.2.36（build 46）

配套固件：**musicA.39**（封面轮播邻封 + 柔光底栏文案；本版 **不改固件**，沿用 musicA.39 Draft）

## Path A · 系统 TabView 液态玻璃壳

按 [`nav-tabview-shell-mockup.md`](../docs 见 Agent Store) 落地：

- **iOS 26+**：用系统 `TabView` 替换自定义悬浮底栏；浮动 Liquid Glass / 选中态由系统提供；可选 `.tabBarMinimizeBehavior(.onScrollDown)`。
- **iOS 15–25**：回退原自定义底栏 / Duo 竖轨（soft-wash），功能可用。
- **Tabs**：导航 / 行程 / 圆屏 / 我的（4，与现网一致）。
- **连接动画**：`ConnectSheetView` overlay **完整保留**。
- **Duo**：`preferSidebar`（原竖轨判定）时 `.tabViewStyle(.sidebarAdaptable)` — 侧栏贴 **信号/电量同侧**；不自绘左内容区游离竖轨、不用 Duo 底栏 Tab。
- **禁止**自定义 `UITabBarAppearance` 不透明背景；启动时恢复 `configureWithDefaultBackground()` 默认外观，避免扼杀系统玻璃。

## 配对

| | |
|--|--|
| iOS | **0.16.2.36** (build 46) |
| Firmware | **7.12.10-ui.2-zelda40.9-musicA.39**（不变） |

音乐样式五主题与 musicA.39 轮播/柔光修复仍有效；刷机包继续用 musicA.39 Draft。
