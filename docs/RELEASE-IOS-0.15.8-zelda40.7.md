# MotoNav iOS v0.15.8-zelda40.7（塞尔达40周年特别版修订 7）

## 这一版真正解决「选项卡纯白」

前六版在「原生 glassEffect 采样发暗」和「白色压层变成纯白平板」之间来回摇摆。
根因不是少调一次透明度，而是两条路都走不通：

1. 特别版固定深色旗帜背景；`glassEffect` 对背后采样 → 选项卡发暗，且不理 `colorScheme`。
2. 用白色压层 0.48~0.55「补白天」→ 视觉上就是纯白死板；设置页 `UISegmentedControl` 半透明白在亮卡片上同样近似死白。

### 需求对照

| 需求 | 本版做法 |
| --- | --- |
| 强制白天模式，不启动自动夜间模式 | 根视图 `.preferredColorScheme(.light)` + `UIWindow.overrideUserInterfaceStyle = .light`，UIKit/SwiftUI 双锁浅色 |
| 选项卡底色 = 液态玻璃透明 | `liquidGlass(light: true)` **跳过**原生 glassEffect，改用强制浅色 `ultraThinMaterial` + 极淡白洗（0.14） |
| 文字颜色相应适配 | 选项卡 / 按钮强制 `.environment(\.colorScheme, .light)`，语义色取浅色系深字 |

### 具体改动

1. **`AppTheme.liquidGlass`**：`light == true` 时走 `daytimeFrost`（毛玻璃模糊 + 0.14 白洗），不再碰原生采样，也不再塞 0.48 死白。
2. **新增 `GlassSegmented`**：设置页「刷新速度 / 预判 / 走过的路线 / 夜间配色 / 导航条摆位」以及独立路线「出行方式」全部换掉 `UISegmentedControl`，底色与底部导航条同一套白天毛玻璃。
3. **选中软胶囊** `selCapsule`：浅色 alpha 从 0.92 → 0.34，选中项不再把整条导航衬成白条。
4. **外观锁定**：`AppDelegate.lockDaytimeAppearance()` 在启动与 `onAppear` 时把所有 window 锁为 `.light`。

### 验证（Mac 构建后确认）

- 底部导航条 / 右侧竖栏：应能透出背后旗帜纹理，呈半透明白天毛玻璃，而不是纯白块或深色块。
- 设置页分段选项卡：半透明毛玻璃，选中项浅色胶囊，文字为深色。
- 系统切到深色模式后，本 App 界面仍保持白天浅色，不会自动夜间。

## 安装

- 下载 `MotoNav-0.15.8-zelda40.7-unsigned.ipa` 侧载安装。
- 固件无需变动（配套 v7.12.10-ui.2-zelda40.3 即可）。
