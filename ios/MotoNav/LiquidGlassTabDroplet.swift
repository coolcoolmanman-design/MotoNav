import SwiftUI

/// 苹果 Store 选中态：大号透明立体玻璃球罩在选中图标上。
/// 对齐 `media/ios-0.15.39-glass-puck/liquid-glass-target.jpg` /
/// `liquid-glass-droplet-still.png`。
/// 须有可读 3D 球体（Fresnel 边 + 强高光 + 软色散）；禁霓虹粗环、白塑料穹顶、图标下彩条。iOS 15+。
struct LiquidGlassDroplet: View {
    var diameter: CGFloat = 64

    var body: some View {
        ZStack {
            // 通透玻璃体积：中心更清，边缘 Fresnel 更密 → 球体而非扁盘
            Circle()
                .fill(.ultraThinMaterial)
                .opacity(0.78)
                .overlay(
                    Circle().fill(
                        RadialGradient(
                            gradient: Gradient(stops: [
                                .init(color: Color.white.opacity(0.42), location: 0.0),
                                .init(color: Color.white.opacity(0.14), location: 0.22),
                                .init(color: Color(red: 0.75, green: 0.86, blue: 1.0).opacity(0.12), location: 0.48),
                                .init(color: Color.white.opacity(0.08), location: 0.72),
                                .init(color: Color.clear, location: 1.0)
                            ]),
                            center: UnitPoint(x: 0.32, y: 0.26),
                            startRadius: 1,
                            endRadius: diameter * 0.62
                        )
                    )
                )

            // 外 Fresnel 白边（立体轮廓）
            Circle()
                .stroke(
                    LinearGradient(
                        colors: [
                            Color.white.opacity(0.95),
                            Color.white.opacity(0.40),
                            Color.white.opacity(0.70)
                        ],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    ),
                    lineWidth: 2.0
                )

            // 内折射环
            Circle()
                .stroke(Color.white.opacity(0.48), lineWidth: 1.15)
                .padding(diameter * 0.15)

            // 软色散边（细、低饱和；非霓虹粗环）
            Circle()
                .stroke(
                    AngularGradient(
                        colors: [
                            Color(red: 0.30, green: 0.85, blue: 0.98).opacity(0.62),
                            Color(red: 0.58, green: 0.38, blue: 0.92).opacity(0.48),
                            Color(red: 0.95, green: 0.80, blue: 0.32).opacity(0.42),
                            Color(red: 0.38, green: 0.78, blue: 0.95).opacity(0.55),
                            Color(red: 0.30, green: 0.85, blue: 0.98).opacity(0.62)
                        ],
                        center: .center
                    ),
                    lineWidth: 2.0
                )
                .blur(radius: 0.75)
                .opacity(0.88)

            // 外柔光（白，非彩虹晕）
            Circle()
                .stroke(Color.white.opacity(0.32), lineWidth: 6)
                .blur(radius: 3.2)
                .opacity(0.70)

            // 顶缘强镜面高光（目标图关键）
            Capsule()
                .fill(
                    LinearGradient(
                        colors: [
                            Color.white.opacity(0.98),
                            Color.white.opacity(0.60),
                            Color.clear
                        ],
                        startPoint: .leading,
                        endPoint: .trailing
                    )
                )
                .frame(width: diameter * 0.52, height: diameter * 0.10)
                .offset(y: -diameter * 0.27)
                .blur(radius: 0.4)

            Circle()
                .fill(Color.white.opacity(0.78))
                .frame(width: diameter * 0.10, height: diameter * 0.10)
                .offset(x: diameter * 0.18, y: -diameter * 0.15)
                .blur(radius: 0.3)

            // 底部内侧微暗（体积接地；禁彩条）
            Ellipse()
                .fill(Color.black.opacity(0.08))
                .frame(width: diameter * 0.58, height: diameter * 0.18)
                .offset(y: diameter * 0.30)
                .blur(radius: 2.2)
        }
        .frame(width: diameter, height: diameter)
        .shadow(color: Color.black.opacity(0.18), radius: 12, y: 6)
        .shadow(color: Color.black.opacity(0.10), radius: 3, y: 1)
    }
}

/// 底栏：毛玻璃胶囊 + 滑动立体玻璃球；球下图标放大并染系统蓝。
struct LiquidGlassTabBar: View {
    @Binding var page: Int
    var titles: [String]
    var icons: [String]
    var fillIcons: [String]
    var onSelect: (Int) -> Void

    @Namespace private var dropletNS
    private let systemBlue = Color(red: 0.10, green: 0.48, blue: 1.0)

    var body: some View {
        HStack(spacing: 0) {
            ForEach(0..<titles.count, id: \.self) { i in
                tabItem(i)
            }
        }
        .padding(.horizontal, 6)
        .padding(.top, 12)
        .padding(.bottom, 8)
        .background(barChrome)
        .padding(.horizontal, 16)
        .padding(.bottom, 10)
    }

    private var barChrome: some View {
        Capsule()
            .fill(.ultraThinMaterial)
            .overlay(
                Capsule().fill(
                    LinearGradient(
                        colors: [
                            Color.white.opacity(0.42),
                            Color.white.opacity(0.12),
                            Color.clear
                        ],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                )
            )
            .overlay(Capsule().stroke(Color.white.opacity(0.34), lineWidth: 0.7))
            .shadow(color: Color.black.opacity(0.12), radius: 16, y: 6)
    }

    private func tabItem(_ i: Int) -> some View {
        let selected = page == i
        return Button {
            withAnimation(.spring(response: 0.45, dampingFraction: 0.78)) {
                onSelect(i)
            }
        } label: {
            VStack(spacing: 2) {
                ZStack {
                    if selected {
                        LiquidGlassDroplet(diameter: 62)
                            .matchedGeometryEffect(id: "tabDroplet", in: dropletNS)
                            .zIndex(0)
                    }
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: selected ? 22 : 19, weight: .semibold))
                        .foregroundColor(selected ? systemBlue : AppTheme.sub.opacity(0.72))
                        .scaleEffect(selected ? 1.14 : 1.0)
                        .offset(y: selected ? -0.6 : 0)
                        .frame(width: 48, height: 40)
                        .zIndex(1)
                }
                .frame(height: 54)

                Text(titles[i])
                    .font(.system(size: 10, weight: selected ? .semibold : .medium))
                    .foregroundColor(selected ? systemBlue : AppTheme.sub.opacity(0.72))
                    .lineLimit(1)
                    .minimumScaleFactor(0.75)
            }
            .frame(maxWidth: .infinity)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

/// Duo 竖栏：纵向滑动液滴。
struct LiquidGlassTabRail: View {
    @Binding var page: Int
    var titles: [String]
    var icons: [String]
    var fillIcons: [String]
    var systemEdge: CGFloat
    var onSelect: (Int) -> Void

    @Namespace private var dropletNS
    private let systemBlue = Color(red: 0.10, green: 0.48, blue: 1.0)

    var body: some View {
        VStack(spacing: 8) {
            Spacer(minLength: systemEdge > 0 ? 170 : 12)
            ForEach(0..<titles.count, id: \.self) { i in
                railItem(i)
            }
            Spacer(minLength: 12)
        }
        .padding(.vertical, 10)
        .frame(width: max(66, systemEdge))
        .background(
            Capsule()
                .fill(.ultraThinMaterial)
                .overlay(Capsule().stroke(Color.white.opacity(0.28), lineWidth: 0.6))
                .shadow(color: Color.black.opacity(0.08), radius: 12, y: 4)
        )
        .padding(.vertical, 8)
        .padding(.trailing, 6)
    }

    private func railItem(_ i: Int) -> some View {
        let selected = page == i
        return Button {
            withAnimation(.spring(response: 0.45, dampingFraction: 0.78)) {
                onSelect(i)
            }
        } label: {
            VStack(spacing: 2) {
                ZStack {
                    if selected {
                        LiquidGlassDroplet(diameter: 56)
                            .matchedGeometryEffect(id: "railDroplet", in: dropletNS)
                    }
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: selected ? 21 : 19, weight: .semibold))
                        .foregroundColor(selected ? systemBlue : AppTheme.sub.opacity(0.72))
                        .scaleEffect(selected ? 1.12 : 1.0)
                        .frame(width: 46, height: 34)
                        .zIndex(1)
                }
                Text(titles[i])
                    .font(.system(size: 10, weight: selected ? .semibold : .medium))
                    .foregroundColor(selected ? systemBlue : AppTheme.sub.opacity(0.72))
                    .lineLimit(1)
                    .minimumScaleFactor(0.6)
            }
            .frame(width: 58)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
