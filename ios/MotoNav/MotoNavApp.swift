import SwiftUI

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 高德导航界面（AMapNaviDriveView）创建时会调用 UIApplication.shared.delegate.window 判断是不是刘海屏。
/// SwiftUI 默认的 AppDelegate 没有 window，会直接崩溃（iOS v0.1.1 点“开始导航”就闪退的原因），
/// 所以这里补一个 AppDelegate，window 返回当前显示的窗口。
final class AppDelegate: NSObject, UIApplicationDelegate {
    var window: UIWindow? {
        get {
            let wins = UIApplication.shared.connectedScenes
                .compactMap { $0 as? UIWindowScene }
                .flatMap { $0.windows }
            return wins.first { $0.isKeyWindow } ?? wins.first
        }
        set {}
    }

    /// 禁用第三方键盘扩展（v0.15.1）：部分第三方输入法在本 App 内点搜索框不弹出/白屏，
    /// 强制使用系统键盘保证可用；用户在其他 App 里仍可用自己的输入法。
    func application(_ application: UIApplication,
                     shouldAllowExtensionPointIdentifier extensionPointIdentifier: UIApplication.ExtensionPointIdentifier) -> Bool {
        extensionPointIdentifier != .keyboard
    }
}

@main
struct MotoNavApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @ObservedObject private var settings = Settings.shared    // 自动白夜：外观跟随设置（v0.15.0）
    @Environment(\.scenePhase) private var scenePhase

    init() {
        // 高德：key + 隐私合规声明（高德要求在用任何功能前调用；自己用的 App，直接设为已同意）
        AMapServices.shared().apiKey = Settings.shared.amapKeyInUse     // 用户在 App 里填的（iOS v0.7 起没有内置 key；没填时是空的）
        AMapServices.shared().enableHTTPS = true
        AMapNaviManagerConfig.shared().updatePrivacyShow(.didShow, privacyInfo: .didContain)
        AMapNaviManagerConfig.shared().updatePrivacyAgree(.didAgree)
        AMapSearchAPI.updatePrivacyShow(.didShow, privacyInfo: .didContain)
        AMapSearchAPI.updatePrivacyAgree(.didAgree)
        MAMapView.updatePrivacyShow(.didShow, privacyInfo: .didContain)
        MAMapView.updatePrivacyAgree(.didAgree)

        // 圆屏发来的通知
        BleLink.shared.onNotify = { msg in
            let p = msg.split(separator: ",").map(String.init)
            if p.first == "BOOT", p.count >= 2, let id = Int(p[1]) { Settings.shared.bootReply(id); return }
            // musicA.19：表盘选择下线，FACE 回执忽略
            if p.first == "FC2", p.count >= 13 { Settings.shared.faceCfg2Reply(p); return }   // musicA.17 六槽
            if p.first == "FV", p.count >= 2, let id = Int(p[1]) { Settings.shared.vehReply(id); return }
            if p.first == "FC3", p.count >= 5 { Settings.shared.slotModesReply(p); return }   // musicA.37 六槽改造
            if p.first == "BG", p.count >= 5 { Settings.shared.dynBgReply(p); return }        // musicA.37 动态背景
            if p.first == "BT", p.count >= 2, let m = Int(p[1]) { BadgeTextBridge.shared.textReply(m); return }   // musicA.18
            if p.first == "BGD", p.count >= 2 { BackgroundBridge.shared.pushed = false; return }
            if p.first == "RC", p.count >= 2, let on = Int(p[1]) { Settings.shared.coverSpinReply(on); return }
            if p.first == "MS", p.count >= 2, let id = Int(p[1]) { Settings.shared.musicStyleReply(id); return }  // musicA.38
            if p.count >= 3, p[0] == "T", p[1] == "M" { MusicBridge.shared.handle(p[2]); return }
            if p.first == "T", p.count >= 2 {
                switch p[1] {
                case "W": Weather.shared.request()                    // 滑到了天气页
                case "A": break                                       // 按住接电话：iOS 不允许 App 接电话
                default: break
                }
            }
        }
        // 启动自动连接也弹 AirPods 式动画（与手动连接同一套）
        ConnectAnimationCoordinator.shared.connectAutomatically()
        LocationService.shared.start()
        Settings.shared.startDayNight()                 // 夜间配色、亮度按日出日落自动切换（每分钟检查）
        NaviBridge.shared.attach()
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .background(AmapCaptureSurface().frame(width: 1, height: 1))
                .preferredColorScheme(settings.appearanceMode.scheme)
        }
        .onChange(of: scenePhase) { phase in
            // 息屏/后台：继续发已缓存底图/楼块；暂停 SDK 抓图，避免 cancel/resend 抖动。
            MapFeeder.shared.setRenderingAvailable(phase == .active)
        }
    }
}
