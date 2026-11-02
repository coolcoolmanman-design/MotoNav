import SwiftUI

/// 苹果 / visionOS 式选中「透明玻璃球」：滑动、淡色散边、软高光、折射染蓝图标。
/// 对齐 `media/ios-0.15.38-glass-puck/liquid-glass-target.jpg`。
/// 禁：彩虹霓虹环、白塑料穹顶、图标下彩条。iOS 15+ SwiftUI 合成。
struct LiquidGlassDroplet: View {
    var diameter: CGFloat = 58

    var body: some View {
        ZStack {
            // 通透球核：极淡材质，避免乳白塑料穹顶
            Circle()
                .fill(.ultraThinMaterial)
                .opacity(0.55)
                .overlay(
                    Circle().fill(
                        RadialGradient(
                            gradient: Gradient(stops: [
                                .init(color: Color.white.opacity(0.22), location: 0.0),
                                .init(color: Color.white.opacity(0.06), location: 0.42),
                                .init(color: Color(red: 0.72, green: 0.82, blue: 1.0).opacity(0.05), location: 0.78),
                                .init(color: Color.clear, location: 1.0)
                            ]),
                            center: UnitPoint(x: 0.36, y: 0.30),
                            startRadius: 1,
                            endRadius: diameter * 0.55
                        )
                    )
                )

            // 内缘微亮（凸透镜边缘，非霓虹）
            Circle()
                .stroke(Color.white.opacity(0.28), lineWidth: 0.8)
                .padding(diameter * 0.10)

            // 淡色散边：极细、低饱和 cyan / violet / warm yellow，无霓虹环
            Circle()
                .stroke(
                    AngularGradient(
                        colors: [
                            Color(red: 0.45, green: 0.82, blue: 0.95).opacity(0.38),
                            Color(red: 0.62, green: 0.48, blue: 0.92).opacity(0.28),
                            Color(red: 0.95, green: 0.86, blue: 0.48).opacity(0.22),
                            Color(red: 0.45, green: 0.82, blue: 0.95).opacity(0.38)
                        ],
                        center: .center
                    ),
                    lineWidth: 1.1
                )
                .blur(radius: 0.55)
                .opacity(0.70)

            // 外缘极轻晕（再压饱和，避免彩虹光环）
            Circle()
                .stroke(Color.white.opacity(0.18), lineWidth: 2.2)
                .blur(radius: 1.6)
                .opacity(0.55)

            // 顶缘软镜面高光（苹果玻璃球特征）
            Capsule()
                .fill(
                    LinearGradient(
                        colors: [
                            Color.white.opacity(0.78),
                            Color.white.opacity(0.28),
                            Color.clear
                        ],
                        startPoint: .leading,
                        endPoint: .trailing
                    )
                )
                .frame(width: diameter * 0.42, height: diameter * 0.07)
                .offset(y: -diameter * 0.27)
                .blur(radius: 0.6)

            // 次高光点（右上）
            Circle()
                .fill(Color.white.opacity(0.55))
                .frame(width: diameter * 0.08, height: diameter * 0.08)
                .offset(x: diameter * 0.16, y: -diameter * 0.18)
                .blur(radius: 0.4)
        }
        .frame(width: diameter, height: diameter)
        .shadow(color: Color.black.opacity(0.10), radius: 8, y: 3)
    }
}

/// 底栏：毛玻璃胶囊 + 滑动液滴；液滴下图标略放大并染系统蓝（模拟折射）。
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
                            Color.white.opacity(0.36),
                            Color.white.opacity(0.10),
                            Color.clear
                        ],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                )
            )
            .overlay(Capsule().stroke(Color.white.opacity(0.30), lineWidth: 0.7))
            .shadow(color: Color.black.opacity(0.11), radius: 14, y: 5)
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
                        LiquidGlassDroplet(diameter: 54)
                            .matchedGeometryEffect(id: "tabDroplet", in: dropletNS)
                            .zIndex(0)
                    }
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: selected ? 21 : 19, weight: .semibold))
                        .foregroundColor(selected ? systemBlue : AppTheme.sub.opacity(0.72))
                        .scaleEffect(selected ? 1.10 : 1.0)
                        .offset(y: selected ? -0.4 : 0)
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
                        LiquidGlassDroplet(diameter: 50)
                            .matchedGeometryEffect(id: "railDroplet", in: dropletNS)
                    }
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: selected ? 20 : 19, weight: .semibold))
                        .foregroundColor(selected ? systemBlue : AppTheme.sub.opacity(0.72))
                        .scaleEffect(selected ? 1.08 : 1.0)
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
