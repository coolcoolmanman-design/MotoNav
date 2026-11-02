import SwiftUI

/// 苹果 / 网易云式选中「液态玻璃液滴」：可横向滑动、色散边、顶高光、底焦散、折射染蓝图标。
/// iOS 15+：SwiftUI 合成（无 Metal 依赖）；视觉对齐 `media/ios-0.15.37-glass-puck/liquid-glass-ref.jpg`。
struct LiquidGlassDroplet: View {
    var body: some View {
        ZStack {
            // 清透球核（比底栏更通透）
            Circle()
                .fill(.ultraThinMaterial)
                .overlay(
                    Circle().fill(
                        RadialGradient(
                            colors: [
                                Color.white.opacity(0.50),
                                Color.white.opacity(0.18),
                                Color(red: 0.85, green: 0.92, blue: 1.0).opacity(0.10),
                                Color.white.opacity(0.04)
                            ],
                            center: .center,
                            startRadius: 2,
                            endRadius: 34
                        )
                    )
                )

            // 色散边缘：青 → 品红 → 黄（chromatic fringe）
            Circle()
                .stroke(
                    AngularGradient(
                        colors: [
                            Color(red: 0.25, green: 0.85, blue: 1.0).opacity(0.95),
                            Color(red: 0.95, green: 0.35, blue: 0.85).opacity(0.90),
                            Color(red: 1.0, green: 0.85, blue: 0.25).opacity(0.85),
                            Color(red: 0.35, green: 0.95, blue: 0.75).opacity(0.80),
                            Color(red: 0.25, green: 0.85, blue: 1.0).opacity(0.95)
                        ],
                        center: .center
                    ),
                    lineWidth: 2.4
                )
                .blur(radius: 0.35)

            // 外圈更淡的虹彩光晕
            Circle()
                .stroke(
                    AngularGradient(
                        colors: [
                            Color.cyan.opacity(0.55),
                            Color.magenta.opacity(0.45),
                            Color.yellow.opacity(0.40),
                            Color.cyan.opacity(0.55)
                        ],
                        center: .center
                    ),
                    lineWidth: 5
                )
                .blur(radius: 2.2)
                .opacity(0.85)

            // 顶部镜面高光
            Ellipse()
                .fill(
                    LinearGradient(
                        colors: [Color.white.opacity(0.95), Color.white.opacity(0.15), .clear],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                )
                .frame(width: 34, height: 16)
                .offset(x: -4, y: -16)
                .blur(radius: 0.6)

            // 底部棱镜焦散
            HStack(spacing: 3) {
                Capsule().fill(Color.cyan.opacity(0.85)).frame(width: 7, height: 3)
                Capsule().fill(Color.yellow.opacity(0.80)).frame(width: 9, height: 3)
                Capsule().fill(Color.magenta.opacity(0.75)).frame(width: 6, height: 3)
            }
            .blur(radius: 0.8)
            .offset(y: 18)

            // 内折射环
            Circle()
                .stroke(Color.white.opacity(0.35), lineWidth: 1)
                .padding(5)
        }
        .frame(width: 56, height: 56)
        .shadow(color: Color.black.opacity(0.12), radius: 8, y: 3)
        .shadow(color: Color.cyan.opacity(0.18), radius: 6, y: 0)
    }
}

/// 底栏：毛玻璃胶囊 + 滑动液滴选中指示（matchedGeometry）。
struct LiquidGlassTabBar: View {
    @Binding var page: Int
    var titles: [String]
    var icons: [String]
    var fillIcons: [String]
    var onSelect: (Int) -> Void

    @Namespace private var dropletNS

    var body: some View {
        HStack(spacing: 0) {
            ForEach(0..<titles.count, id: \.self) { i in
                tabItem(i)
            }
        }
        .padding(.horizontal, 8)
        .padding(.top, 10)
        .padding(.bottom, 8)
        .background(barChrome)
        .padding(.horizontal, 18)
        .padding(.bottom, 10)
    }

    private var barChrome: some View {
        Capsule()
            .fill(.ultraThinMaterial)
            .overlay(
                Capsule().fill(
                    LinearGradient(
                        colors: [
                            Color.white.opacity(0.38),
                            Color.white.opacity(0.10),
                            Color.clear
                        ],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                )
            )
            .overlay(Capsule().stroke(Color.white.opacity(0.32), lineWidth: 0.7))
            .shadow(color: Color.black.opacity(0.10), radius: 14, y: 5)
    }

    private func tabItem(_ i: Int) -> some View {
        let selected = page == i
        return Button {
            withAnimation(.spring(response: 0.42, dampingFraction: 0.82)) {
                onSelect(i)
            }
        } label: {
            VStack(spacing: 3) {
                ZStack {
                    if selected {
                        LiquidGlassDroplet()
                            .matchedGeometryEffect(id: "tabDroplet", in: dropletNS)
                            .transition(.scale.combined(with: .opacity))
                    }
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: selected ? 21 : 19, weight: .semibold))
                        // 液滴下图标：鲜明系统蓝（折射染色）
                        .foregroundColor(selected ? Color(red: 0.10, green: 0.48, blue: 1.0) : AppTheme.sub.opacity(0.72))
                        .scaleEffect(selected ? 1.08 : 1.0)
                        .shadow(color: selected ? Color.blue.opacity(0.35) : .clear, radius: 3, y: 0)
                        .frame(width: 44, height: 36)
                        .zIndex(1)
                }
                .frame(height: 44)

                Text(titles[i])
                    .font(.system(size: 10, weight: selected ? .semibold : .medium))
                    .foregroundColor(selected ? Color(red: 0.10, green: 0.48, blue: 1.0) : AppTheme.sub.opacity(0.72))
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
            withAnimation(.spring(response: 0.42, dampingFraction: 0.82)) {
                onSelect(i)
            }
        } label: {
            VStack(spacing: 2) {
                ZStack {
                    if selected {
                        LiquidGlassDroplet()
                            .scaleEffect(0.92)
                            .matchedGeometryEffect(id: "railDroplet", in: dropletNS)
                    }
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: 20, weight: .semibold))
                        .foregroundColor(selected ? Color(red: 0.10, green: 0.48, blue: 1.0) : AppTheme.sub.opacity(0.72))
                        .frame(width: 46, height: 34)
                        .zIndex(1)
                }
                Text(titles[i])
                    .font(.system(size: 10, weight: selected ? .semibold : .medium))
                    .foregroundColor(selected ? Color(red: 0.10, green: 0.48, blue: 1.0) : AppTheme.sub.opacity(0.72))
                    .lineLimit(1)
                    .minimumScaleFactor(0.6)
            }
            .frame(width: 58)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
