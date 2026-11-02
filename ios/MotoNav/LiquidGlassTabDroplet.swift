import SwiftUI

/// 苹果 / 网易云式选中「液态玻璃球体」：滑动、色散边、顶高光、底焦散、折射放大并染蓝图标。
/// 对齐 `media/ios-0.15.37-glass-puck/liquid-glass-ref.jpg`。iOS 15+ SwiftUI 合成。
struct LiquidGlassDroplet: View {
    var diameter: CGFloat = 58

    var body: some View {
        ZStack {
            // 体积球核：通透玻璃（比底栏更清）
            Circle()
                .fill(.ultraThinMaterial)
                .overlay(
                    Circle().fill(
                        RadialGradient(
                            gradient: Gradient(stops: [
                                .init(color: Color.white.opacity(0.55), location: 0.0),
                                .init(color: Color.white.opacity(0.18), location: 0.35),
                                .init(color: Color(red: 0.78, green: 0.90, blue: 1.0).opacity(0.12), location: 0.7),
                                .init(color: Color.white.opacity(0.04), location: 1.0)
                            ]),
                            center: UnitPoint(x: 0.38, y: 0.28),
                            startRadius: 1,
                            endRadius: diameter * 0.55
                        )
                    )
                )

            // 内折射环（凸透镜边缘）
            Circle()
                .stroke(Color.white.opacity(0.45), lineWidth: 1.2)
                .padding(diameter * 0.12)

            // 色散虹边（chromatic fringe）
            Circle()
                .stroke(
                    AngularGradient(
                        colors: [
                            Color(red: 0.15, green: 0.90, blue: 1.0),
                            Color(red: 1.0, green: 0.25, blue: 0.85),
                            Color(red: 1.0, green: 0.88, blue: 0.20),
                            Color(red: 0.20, green: 1.0, blue: 0.65),
                            Color(red: 0.15, green: 0.90, blue: 1.0)
                        ],
                        center: .center
                    ),
                    lineWidth: 2.6
                )
                .blur(radius: 0.4)

            // 外虹光晕
            Circle()
                .stroke(
                    AngularGradient(
                        colors: [
                            Color(red: 0.0, green: 0.85, blue: 0.95).opacity(0.55),
                            Color(red: 1.0, green: 0.20, blue: 0.75).opacity(0.40),
                            Color(red: 1.0, green: 0.88, blue: 0.15).opacity(0.35),
                            Color(red: 0.0, green: 0.85, blue: 0.95).opacity(0.55)
                        ],
                        center: .center
                    ),
                    lineWidth: 6
                )
                .blur(radius: 2.4)
                .opacity(0.75)

            // 顶缘横向镜面高光（参考图关键高光条）
            Capsule()
                .fill(
                    LinearGradient(
                        colors: [Color.white.opacity(0.95), Color.white.opacity(0.35), .clear],
                        startPoint: .leading,
                        endPoint: .trailing
                    )
                )
                .frame(width: diameter * 0.55, height: diameter * 0.10)
                .offset(y: -diameter * 0.28)
                .blur(radius: 0.5)

            // 底部棱镜焦散点
            HStack(spacing: 2.5) {
                Capsule().fill(Color(red: 0.0, green: 0.85, blue: 0.95).opacity(0.9)).frame(width: 8, height: 3.5)
                Capsule().fill(Color(red: 1.0, green: 0.88, blue: 0.15).opacity(0.85)).frame(width: 10, height: 3.5)
                Capsule().fill(Color(red: 1.0, green: 0.20, blue: 0.75).opacity(0.8)).frame(width: 7, height: 3.5)
            }
            .blur(radius: 0.7)
            .offset(y: diameter * 0.30)
        }
        .frame(width: diameter, height: diameter)
        .shadow(color: Color.black.opacity(0.14), radius: 10, y: 4)
        .shadow(color: Color(red: 0.0, green: 0.85, blue: 0.95).opacity(0.20), radius: 5, y: 0)
    }
}

/// 底栏：毛玻璃胶囊 + 滑动液滴；液滴下图标放大并染系统蓝（模拟折射）。
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
        .padding(.top, 8)
        .padding(.bottom, 7)
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
                            Color.white.opacity(0.40),
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
                    // 液滴在图标后方/环绕：先画液滴，再画被「折射放大」的图标
                    if selected {
                        LiquidGlassDroplet(diameter: 56)
                            .matchedGeometryEffect(id: "tabDroplet", in: dropletNS)
                            .zIndex(0)
                    }
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: selected ? 22 : 19, weight: .semibold))
                        .foregroundColor(selected ? systemBlue : AppTheme.sub.opacity(0.72))
                        // 凸透镜：选中图标略放大 + 微位移模拟折射
                        .scaleEffect(selected ? 1.14 : 1.0)
                        .offset(y: selected ? -0.5 : 0)
                        .shadow(color: selected ? systemBlue.opacity(0.35) : .clear, radius: 4, y: 0)
                        .frame(width: 48, height: 40)
                        .zIndex(1)
                }
                .frame(height: 48)

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
                        LiquidGlassDroplet(diameter: 52)
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
