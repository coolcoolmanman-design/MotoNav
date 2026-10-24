import SwiftUI

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.13.9 新增）
///
/// 自动白夜模式（和安卓版对齐后新增）：
/// - 界面的语义色不写死，而是浅色 / 深色两套值，用 UIColor 的 dynamic Provider 包起来
/// - 跟随系统深浅色自动切换；用户在「圆屏设置 → 外观」里也可以强制浅色或深色
/// - 深色沿用「罪恶都市风」（和安卓 NavStyle 一致），浅色是同一套结构的白底版本
extension Color {
    /// 浅色 / 深色两套值，跟随当前界面样式自动切换
    static func dynamic(light: UIColor, dark: UIColor) -> Color {
        Color(UIColor { traits in
            traits.userInterfaceStyle == .dark ? dark : light
        })
    }
}

/// 语义色板：界面里只用这些名字，不直接写死颜色
enum AppTheme {
    // MARK: 页面
    /// 页面背景
    static let bg = Color.dynamic(
        light: UIColor(red: 0.957, green: 0.945, blue: 0.973, alpha: 1),   // #F4F1F8
        dark:  UIColor(red: 0.055, green: 0.040, blue: 0.080, alpha: 1))   // #0E0A14 罪恶都市底
    /// 卡片
    static let card = Color.dynamic(
        light: UIColor(red: 1, green: 1, blue: 1, alpha: 1),               // #FFFFFF
        dark:  UIColor(red: 0.102, green: 0.078, blue: 0.141, alpha: 1))   // #1A1424
    /// 卡片内嵌的浅底（输入框、路线卡片的凹槽）
    static let sunken = Color.dynamic(
        light: UIColor(red: 0.945, green: 0.933, blue: 0.969, alpha: 1),   // #F1EEF7
        dark:  UIColor(red: 0.055, green: 0.040, blue: 0.080, alpha: 1))   // #0E0A14
    /// 描边
    static let stroke = Color.dynamic(
        light: UIColor(red: 0.851, green: 0.824, blue: 0.902, alpha: 1),   // #D9D2E6
        dark:  UIColor(red: 0.227, green: 0.180, blue: 0.298, alpha: 1))   // #3A2E4C

    // MARK: 文字
    /// 主文字
    static let text = Color.dynamic(
        light: UIColor(red: 0.102, green: 0.078, blue: 0.141, alpha: 1),   // #1A1424
        dark:  UIColor(red: 0.953, green: 0.933, blue: 0.973, alpha: 1))   // #F3EEF8
    /// 次要文字
    static let sub = Color.dynamic(
        light: UIColor(red: 0.420, green: 0.380, blue: 0.471, alpha: 1),   // #6B6178
        dark:  UIColor(red: 0.655, green: 0.616, blue: 0.710, alpha: 1))   // #A79DB6

    // MARK: 强调色（罪恶都市风：粉 + 青）
    static let pink = Color.dynamic(
        light: UIColor(red: 0.898, green: 0.129, blue: 0.624, alpha: 1),   // #E5219F
        dark:  UIColor(red: 1.0, green: 0.310, blue: 0.706, alpha: 1))     // #FF4FB4
    static let cyan = Color.dynamic(
        light: UIColor(red: 0.047, green: 0.561, blue: 0.639, alpha: 1),   // #0C8FA3
        dark:  UIColor(red: 0.263, green: 0.882, blue: 0.941, alpha: 1))   // #43E1F0
    /// 渐变（标题 MotoNav 用）
    static let titleGradient = LinearGradient(
        colors: [pink, cyan], startPoint: .leading, endPoint: .trailing)

    // MARK: 状态色（两种模式下都用同一套，保证语义明确）
    static let green = Color(red: 0.24, green: 0.86, blue: 0.52)           // #3DDC84
    static let amber = Color(red: 1.0, green: 0.71, blue: 0.28)            // #FFB547
    static let red = Color(red: 1.0, green: 0.30, blue: 0.37)              // #FF4D5E
    static let purple = Color(red: 0.56, green: 0.36, blue: 1.0)           // #8E5CFF
    static let orange = Color(red: 1.0, green: 0.60, blue: 0.20)
}

/// 外观模式：0 跟随系统（默认），1 始终浅色，2 始终深色
enum AppearanceMode: Int, CaseIterable {
    case system = 0
    case light = 1
    case dark = 2

    var name: String {
        switch self {
        case .system: return "跟随系统"
        case .light: return "浅色"
        case .dark: return "深色"
        }
    }

    /// MotoNavApp 里 .preferredColorScheme 用
    var scheme: ColorScheme? {
        switch self {
        case .system: return nil
        case .light: return .light
        case .dark: return .dark
        }
    }
}

// MARK: - 液态玻璃（iOS 26 Liquid Glass；旧系统降级为毛玻璃材质）

extension View {
    /// Tab 条 / 浮层用：iOS 26+ 走系统液态玻璃 glassEffect，旧系统用 ultraThinMaterial 顶上
    @ViewBuilder
    func liquidGlass<S: Shape>(_ shape: S) -> some View {
        if #available(iOS 26.0, *) {
            self.glassEffect(.regular, in: shape)
        } else {
            self.background(
                shape.fill(.ultraThinMaterial)
                    .overlay(shape.stroke(AppTheme.stroke.opacity(0.45), lineWidth: 0.5))
            )
        }
    }

    /// 选中态的小胶囊：玻璃上再叠一层强调色玻璃
    @ViewBuilder
    func liquidGlassTint(_ shape: some Shape, active: Bool, tint: Color) -> some View {
        if #available(iOS 26.0, *), active {
            self.glassEffect(.regular.tint(tint.opacity(0.28)).interactive(), in: shape)
        } else {
            self.background(
                shape.fill(active ? tint.opacity(0.16) : Color.clear)
            )
        }
    }
}
