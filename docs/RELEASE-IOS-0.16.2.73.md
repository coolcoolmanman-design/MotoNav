# iOS 0.16.2.73

基线 0.16.2.72（`231ee48` / Draft `untagged-a2d3f4644d600e674c4f`）实机安装闪退修复。
**仅 App**；固件仍 `7.12.10-ui.2-zelda40.9-musicA.79`。Draft 不 publish。

## 根因

0.16.2.72 起行程 / 圆屏 Tab 在 `wide==true` 时换根视图。冷启动 `GeometryReader` 常先给 **0×0**（`wide=false`）再跳到 Duo/iPad 真机尺寸（`wide=true`），Path A 预建的四 Tab 在首帧发生根视图翻转，触发 SwiftUI AttributeGraph 崩溃 → 实机闪退。
另：宽屏圆屏设置入口用 `onAppear` 改父级 `roundSettingsTab`、以及 `NavigationLink` 急切展开 settings 大树，加重首帧风险。

## 修复

- 几何未就绪（宽/高 ≤100）时只铺底，就绪后再建 Tab 壳，避免 0→真机 wide/rail 翻转。
- 行程 / 圆屏统一入口（`TripsOverviewView(wideSplit:)` / `deviceTabContent(wide:)`），不再 `else if wide` 换根。
- 圆屏设置预选改为本地 `@State`（`RoundSettingsPrefillPage`），`LazyView` 延后 destination。
- SF Symbol `rotate.3d` → `rotate.right`（iOS 15 稳妥）。
- TPMS：`start` / `forceResend` / 回调内重扫改 async；`allowDuplicates` 用 `NSNumber`。
- `destCard` ViewBuilder 分支用 `Group`/`VStack` 包齐；`foregroundStyle` → `foregroundColor`。

## 版本

- `MARKETING_VERSION` → `0.16.2.73`
- `CURRENT_PROJECT_VERSION` → `82`

## 建议实机验证

1. 卸载 0.16.2.72 后侧载本 IPA，冷启动应不再闪退（Duo 合盖/展开、直板机均测）。
2. Duo 展开：行程左右分栏、圆屏右设置入口仍可用。
3. TPMS 前台仍应立刻无过滤扫描。
