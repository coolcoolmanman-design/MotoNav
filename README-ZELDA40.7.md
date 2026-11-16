# MotoNav iOS · 塞尔达40周年特别版 zelda40.7

本仓库包含从 [ios-v0.15.8-zelda40.6](https://github.com/coolcoolmanman-design/MotoNav/releases/tag/ios-v0.15.8-zelda40.6) 起修订的 iOS 客户端源码。

## 本版解决的问题

前 6 次调整后，选项卡底色仍是纯白。zelda40.7 定案：

- **强制白天模式**：`Info.plist` / `preferredColorScheme` / `UIWindow.overrideUserInterfaceStyle` 三处锁浅色，不跟随系统自动夜间。
- **选项卡底色 = 液态毛玻璃透明**：跳过会对深色旗帜背景采样发暗的原生 `glassEffect`，也跳过会变成死白的高不透明白压层；改用强制浅色 `ultraThinMaterial` + 0.14 极淡白洗。
- **文字适配**：选项卡与导航条强制浅色语义（深色字）。
- **分段选项卡**：设置页换成自定义 `GlassSegmented`，不再用死白的 `UISegmentedControl`。

详见 [docs/RELEASE-IOS-0.15.8-zelda40.7.md](docs/RELEASE-IOS-0.15.8-zelda40.7.md)。

## 在 Mac 上构建

需要 macOS + Xcode + CocoaPods：

```bash
cd ios
pod install
cd ..
./scripts/build-ios-ipa.sh
```

产物在 `output/ios/`。侧载安装见 [docs/INSTALL-IOS.md](docs/INSTALL-IOS.md)。

本环境为 Linux，无法在此直接编译出 IPA；请在 Mac 上按上面步骤打包验证。
