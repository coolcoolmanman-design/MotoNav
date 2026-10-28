import SwiftUI

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.13.9 新增）
///
/// 自动白夜模式（和安卓版对齐后新增）：
/// - 界面的语义色不写死，而是浅色 / 深色两套值，用 UIColor 的 dynamic Provider 包起来
/// - 跟随系统深浅色自动切换；用户在「圆屏设置 → 外观」里也可以强制浅色或深色
/// - 深色沿用「罪恶都市风」（和安卓 NavStyle 一致），浅色是同一套结构的白底版本
///
/// iOS v0.15.3 液态玻璃配色优化：
/// - 粉 / 青双强调色保留品牌身份，按玻璃场景重新调校明度与饱和度
///   （浅色更深一点保证文字对比，深色更亮一点让玻璃上的强调色“发光”）
/// - 暗色底去掉一点紫，压得更深，让透明玻璃的层次更干净
/// - 新增玻璃按钮体系 GlassButton / GlassChip：透明玻璃底，颜色只靠 tint 淡染
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
        light: UIColor(red: 0.960, green: 0.949, blue: 0.976, alpha: 1),   // #F5F2F9
        dark:  UIColor(red: 0.043, green: 0.035, blue: 0.063, alpha: 1))   // #0B0910 罪恶都市底（减紫压深）
    /// 卡片（内容层：保持实色，玻璃只用在控制层）
    static let card = Color.dynamic(
        light: UIColor(red: 1, green: 1, blue: 1, alpha: 1),               // #FFFFFF
        dark:  UIColor(red: 0.090, green: 0.075, blue: 0.122, alpha: 1))   // #17131F
    /// 卡片内嵌的浅底（输入框、路线卡片的凹槽）
    static let sunken = Color.dynamic(
        light: UIColor(red: 0.937, green: 0.925, blue: 0.961, alpha: 1),   // #EFECF5
        dark:  UIColor(red: 0.055, green: 0.045, blue: 0.078, alpha: 1))   // #0E0B14
    /// 描边
    static let stroke = Color.dynamic(
        light: UIColor(red: 0.863, green: 0.835, blue: 0.910, alpha: 1),   // #DCD5E8
        dark:  UIColor(red: 0.196, green: 0.165, blue: 0.259, alpha: 1))   // #322A42

    // MARK: 文字
    /// 主文字
    static let text = Color.dynamic(
        light: UIColor(red: 0.098, green: 0.075, blue: 0.137, alpha: 1),   // #191323
        dark:  UIColor(red: 0.957, green: 0.941, blue: 0.976, alpha: 1))   // #F4F0F9
    /// 次要文字
    static let sub = Color.dynamic(
        light: UIColor(red: 0.420, green: 0.384, blue: 0.502, alpha: 1),   // #6B6280
        dark:  UIColor(red: 0.659, green: 0.624, blue: 0.722, alpha: 1))   // #A89FB8
    /// 导航条选中项软胶囊：半透明材质感，禁止再回到死白实心
    /// zelda40.8：白度再降，靠玻璃本身层次，而不是白块衬底
    static let selCapsule = Color.dynamic(
        light: UIColor(white: 1, alpha: 0.16),
        dark:  UIColor(white: 1, alpha: 0.10))

    // MARK: 强调色（罪恶都市风：粉 + 青，玻璃场景调校版）
    static let pink = Color.dynamic(
        light: UIColor(red: 0.898, green: 0.129, blue: 0.624, alpha: 1),   // #E5219F
        dark:  UIColor(red: 1.0, green: 0.435, blue: 0.769, alpha: 1))     // #FF6FC4 玻璃上更亮的粉
    static let cyan = Color.dynamic(
        light: UIColor(red: 0.055, green: 0.592, blue: 0.675, alpha: 1),   // #0E97AC
        dark:  UIColor(red: 0.384, green: 0.914, blue: 0.961, alpha: 1))   // #62E9F5 玻璃上更亮的青

    // MARK: Tab 选中态功能色（v0.15.8-zelda40.8：4 Tab · 出行 / 收藏 / 圆屏 / 我的）
    static let tabAccents: [Color] = [
        pink,                                                              // 出行
        cyan,                                                              // 收藏
        Color.dynamic(
            light: UIColor(red: 0.686, green: 0.322, blue: 0.871, alpha: 1), // #AF52DE 圆屏
            dark:  UIColor(red: 0.784, green: 0.451, blue: 0.941, alpha: 1)),
        Color.dynamic(
            light: UIColor(red: 0.204, green: 0.659, blue: 0.325, alpha: 1), // #34A853 我的
            dark:  UIColor(red: 0.298, green: 0.831, blue: 0.486, alpha: 1)),
    ]

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

// MARK: - 液态玻璃（iOS 26 Liquid Glass；旧系统降级毛玻璃）

extension View {
    /// Tab 条 / 浮层 / 按钮用的液态玻璃。
    /// - tint：淡染的强调色；interactive：可按压（原生 glassEffect）
    /// - light：白天观感偏置（强制浅色语义下的导航条 / 芯片）。
    ///
    /// zelda40.8 定案（纠正 40.7）：
    /// - **恢复** iOS 26 原生 `glassEffect`，内容滚动时实时折射 / 模糊，不再因「防发暗」整段关掉液态玻璃。
    /// - **禁止** 任何白色实心压层（0.14~0.55 都会在亮卡片 / 浅色材质上看成纯白平板）。
    /// - `light` 仅用极淡白 tint（≈0.06）把采样略推向白天，不破坏通透；旧系统只用
    ///   `ultraThinMaterial` + 描边，同样不加白洗。
    @ViewBuilder
    func liquidGlass<S: Shape>(_ shape: S, tint: Color? = nil, interactive: Bool = false, light: Bool = false) -> some View {
        if #available(iOS 26.0, *) {
            self.glassEffect(Self.glassConfig(tint: tint, interactive: interactive, lightBias: light), in: shape)
        } else {
            self.background {
                shape.fill(.ultraThinMaterial)
                    .environment(\.colorScheme, light ? .light : .dark)
                    .overlay {
                        if let tint { shape.fill(tint.opacity(0.10)) }
                    }
                    .overlay(shape.stroke(Color.white.opacity(light ? 0.35 : 0.22), lineWidth: 0.6))
            }
        }
    }

    /// 兼容旧调用
    func lightSegmented() -> some View { self.environment(\.colorScheme, .light) }

    @available(iOS 26.0, *)
    private static func glassConfig(tint: Color?, interactive: Bool, lightBias: Bool) -> Glass {
        var glass = Glass.regular
        if let tint {
            glass = glass.tint(tint)
        } else if lightBias {
            // 极淡白偏置：白天通透，不做不透明白板
            glass = glass.tint(Color.white.opacity(0.06))
        }
        if interactive { glass = glass.interactive() }
        return glass
    }

    /// 选中态小胶囊：强调色淡染，不用白块
    @ViewBuilder
    func liquidGlassTint(_ shape: some Shape, active: Bool, tint: Color) -> some View {
        if active {
            self.background(shape.fill(tint.opacity(0.16)))
        } else {
            self
        }
    }
}

// MARK: - 玻璃分段选项卡（卡片内；不做白色液态板）

/// zelda40.8：设置页分段选项卡。
/// 卡片已是浅色实底，再叠「白玻璃」必然纯白；这里改为凹陷材质轨道 + 强调色选中态，
/// 文字浅色语义（深字），与底部导航条的原生液态玻璃分工明确。
struct GlassSegmented<Selection: Hashable>: View {
    let options: [(Selection, String)]
    @Binding var selection: Selection

    var body: some View {
        HStack(spacing: 2) {
            ForEach(Array(options.enumerated()), id: \.offset) { _, item in
                let selected = selection == item.0
                Button {
                    selection = item.0
                } label: {
                    Text(item.1)
                        .font(.subheadline.weight(selected ? .semibold : .medium))
                        .foregroundStyle(selected ? AppTheme.text : AppTheme.sub)
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 7)
                        .padding(.horizontal, 4)
                        .background {
                            if selected {
                                Capsule()
                                    .fill(AppTheme.pink.opacity(0.14))
                            }
                        }
                        .contentShape(Capsule())
                }
                .buttonStyle(.plain)
            }
        }
        .padding(3)
        .background {
            Capsule()
                .fill(AppTheme.sunken.opacity(0.92))
                .overlay(Capsule().stroke(AppTheme.stroke.opacity(0.7), lineWidth: 0.6))
        }
        .environment(\.colorScheme, .light)
    }
}

// MARK: - 玻璃按钮（iOS v0.15.3 所有按钮统一液态玻璃、透明配色）
//
// 设计口径（对齐 Apple HIG / iOS 26 Liquid Glass 最佳实践）：
// - 玻璃只出现在「控制层」：按钮、Tab 条、浮层；卡片等内容层保持实色，不玻璃叠玻璃
// - 按钮一律透明玻璃底，不用实色大色块；颜色只靠 tint 淡染，一屏一个 prominent 主操作
// - iOS 26 用原生 glassEffect（.interactive() 支持按压反馈），旧系统降级 ultraThinMaterial

/// 玻璃按钮的浓淡：clear 纯透明 / tinted 主操作淡染 / prominent 唯一主 CTA / destructive 危险操作
enum GlassButtonStyle {
    case clear
    case tinted
    case prominent
    case destructive
}

/// 统一的液态玻璃按钮：透明玻璃胶囊，图标 + 文字
struct GlassButton: View {
    let title: String
    var icon: String? = nil
    var style: GlassButtonStyle = .clear
    var tint: Color = AppTheme.pink
    var enabled = true
    var compact = false
    let action: () -> Void

    /// 玻璃上淡染的强调色强度（prominent 是唯一“重”的，其余都轻）
    private var tintOpacity: Double {
        switch style {
        case .clear: return 0
        case .tinted, .destructive: return 0.22
        case .prominent: return 0.42
        }
    }

    /// zelda40.3 白夜玻璃上叠加的品牌色提示层浓度（白底玻璃 + 淡色层 = 白夜模式的浅色按钮）
    private var tintFillOpacity: Double {
        switch style {
        case .clear: return 0
        case .tinted, .destructive: return 0.16
        case .prominent: return 0.26
        }
    }

    private var glassTint: Color? {
        switch style {
        case .clear: return nil
        case .destructive: return AppTheme.red
        default: return tint
        }
    }

    private var foreground: Color {
        switch style {
        case .destructive: return AppTheme.red
        default: return AppTheme.text
        }
    }

    var body: some View {
        Button(action: action) {
            Group {
                if let icon {
                    Label(title, systemImage: icon)
                } else {
                    Text(title)
                }
            }
            .font(compact ? .subheadline.weight(.semibold) : .headline)
            .foregroundStyle(foreground)
            .lineLimit(1)
            .minimumScaleFactor(0.8)
            .padding(.horizontal, compact ? 12 : 18)
            .padding(.vertical, compact ? 8 : 12)
            .frame(minHeight: compact ? 32 : 46)
            .background {
                if tintFillOpacity > 0, let gt = glassTint {
                    Capsule().fill(gt.opacity(tintFillOpacity))    // zelda40.3：品牌色提示层叠在白玻璃上
                }
            }
            .liquidGlass(Capsule(), interactive: true, light: true)
            .environment(\.colorScheme, .light)   // zelda40.3：语义色/强调色取浅色系（深字），保证白玻璃上的对比
            .contentShape(Capsule())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.45)
    }
}

/// 玻璃小芯片：图标 + 短文字（或纯文字），用于行内次要操作和浮层控制条
/// - compact：更小一档（caption 字重），用于行内密集操作（收藏、清除、移除等）
struct GlassChip: View {
    let title: String
    var icon: String? = nil
    var tint: Color? = nil
    var foreground: Color? = nil
    var enabled = true
    var compact = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Group {
                if let icon {
                    Label(title, systemImage: icon)
                } else {
                    Text(title)
                }
            }
            .font(compact ? .caption.weight(.semibold) : .subheadline.weight(.semibold))
            .foregroundStyle(foreground ?? AppTheme.text)
            .lineLimit(1)
            .minimumScaleFactor(0.75)
            .padding(.horizontal, compact ? 9 : 12)
            .padding(.vertical, compact ? 5 : 8)
            .background {
                if let tint { Capsule().fill(tint.opacity(0.16)) }   // zelda40.3：品牌色提示层
            }
            .liquidGlass(Capsule(), interactive: true, light: true)
            .environment(\.colorScheme, .light)   // zelda40.3：白夜玻璃 + 浅色语义（深字）
            .contentShape(Capsule())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.45)
    }
}
