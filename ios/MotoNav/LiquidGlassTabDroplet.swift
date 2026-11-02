import SwiftUI

/// 苹果 Music / Store 选中态：可滚动的透明玻璃胶囊，罩住图标+文案。
/// 对齐 `media/ios-0.15.40-liquid-glass/tab-liquid-glass-ref.mp4` 与 frame-001…017。
/// 动态 matchedGeometry 滚动；边缘色散/折射/高光；禁霓虹粗环、扁蓝盘、图标下彩条。iOS 15+。
struct LiquidGlassDroplet: View {
    /// 胶囊宽高（罩住单 tab 的图标+标签）。
    var width: CGFloat = 64
    var height: CGFloat = 56

    var body: some View {
        let r = min(width, height) * 0.42
        ZStack {
            // 通透玻璃体积：中心更清，边缘 Fresnel 更密
            RoundedRectangle(cornerRadius: r, style: .continuous)
                .fill(.ultraThinMaterial)
                .opacity(0.82)
                .overlay(
                    RoundedRectangle(cornerRadius: r, style: .continuous)
                        .fill(
                            RadialGradient(
                                gradient: Gradient(stops: [
                                    .init(color: Color.white.opacity(0.48), location: 0.0),
                                    .init(color: Color.white.opacity(0.16), location: 0.28),
                                    .init(color: Color(red: 0.75, green: 0.86, blue: 1.0).opacity(0.10), location: 0.55),
                                    .init(color: Color.white.opacity(0.06), location: 0.78),
                                    .init(color: Color.clear, location: 1.0)
                                ]),
                                center: UnitPoint(x: 0.34, y: 0.22),
                                startRadius: 1,
                                endRadius: max(width, height) * 0.55
                            )
                        )
                )

            // 外 Fresnel 白边
            RoundedRectangle(cornerRadius: r, style: .continuous)
                .stroke(
                    LinearGradient(
                        colors: [
                            Color.white.opacity(0.95),
                            Color.white.opacity(0.38),
                            Color.white.opacity(0.72)
                        ],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    ),
                    lineWidth: 1.6
                )

            // 内折射环
            RoundedRectangle(cornerRadius: r * 0.85, style: .continuous)
                .stroke(Color.white.opacity(0.42), lineWidth: 1.0)
                .padding(max(4, min(width, height) * 0.10))

            // 软色散边（RGB split / iridescent fringe；细、低饱和）
            RoundedRectangle(cornerRadius: r, style: .continuous)
                .stroke(
                    AngularGradient(
                        colors: [
                            Color(red: 0.25, green: 0.88, blue: 0.98).opacity(0.70),
                            Color(red: 0.95, green: 0.35, blue: 0.72).opacity(0.55),
                            Color(red: 0.98, green: 0.82, blue: 0.28).opacity(0.48),
                            Color(red: 0.35, green: 0.78, blue: 0.98).opacity(0.60),
                            Color(red: 0.25, green: 0.88, blue: 0.98).opacity(0.70)
                        ],
                        center: .center
                    ),
                    lineWidth: 1.8
                )
                .blur(radius: 0.65)
                .opacity(0.92)

            // 外柔光（白，非彩虹晕）
            RoundedRectangle(cornerRadius: r, style: .continuous)
                .stroke(Color.white.opacity(0.28), lineWidth: 5)
                .blur(radius: 2.8)
                .opacity(0.65)

            // 顶缘强镜面高光
            Capsule()
                .fill(
                    LinearGradient(
                        colors: [
                            Color.white.opacity(0.98),
                            Color.white.opacity(0.55),
                            Color.clear
                        ],
                        startPoint: .leading,
                        endPoint: .trailing
                    )
                )
                .frame(width: width * 0.48, height: max(4, height * 0.08))
                .offset(y: -height * 0.28)
                .blur(radius: 0.35)

            // 侧高光点
            Circle()
                .fill(Color.white.opacity(0.72))
                .frame(width: max(5, width * 0.10), height: max(5, width * 0.10))
                .offset(x: width * 0.18, y: -height * 0.12)
                .blur(radius: 0.25)

            // 底部内侧微暗（体积接地；禁彩条）
            Ellipse()
                .fill(Color.black.opacity(0.07))
                .frame(width: width * 0.55, height: height * 0.16)
                .offset(y: height * 0.28)
                .blur(radius: 1.8)
        }
        .frame(width: width, height: height)
        .shadow(color: Color.black.opacity(0.16), radius: 10, y: 4)
        .shadow(color: Color.black.opacity(0.08), radius: 2, y: 1)
    }
}

/// 底栏：毛玻璃胶囊 + 滚动玻璃气泡（罩住图标+文案）；选中染系统蓝，未选中黑色。
/// 切换时水平拉伸模拟液体滚动（对齐 ref 视频 morph/roll）。
struct LiquidGlassTabBar: View {
    @Binding var page: Int
    var titles: [String]
    var icons: [String]
    var fillIcons: [String]
    var onSelect: (Int) -> Void

    @Namespace private var dropletNS
    @State private var isRolling = false
    private let systemBlue = Color(red: 0.10, green: 0.48, blue: 1.0)
    private let inactive = Color.black.opacity(0.88)

    var body: some View {
        HStack(spacing: 0) {
            ForEach(0..<titles.count, id: \.self) { i in
                tabItem(i)
            }
        }
        .padding(.horizontal, 6)
        .padding(.vertical, 6)
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

    private func selectTab(_ i: Int) {
        guard i != page else { return }
        isRolling = true
        withAnimation(.spring(response: 0.42, dampingFraction: 0.62)) {
            onSelect(i)
        }
        // 行程末段收回拉伸，落到静止胶囊（表面张力回弹感）
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.28) {
            withAnimation(.spring(response: 0.32, dampingFraction: 0.78)) {
                isRolling = false
            }
        }
    }

    private func tabItem(_ i: Int) -> some View {
        let selected = page == i
        return Button {
            selectTab(i)
        } label: {
            ZStack {
                if selected {
                    LiquidGlassDroplet(width: 68, height: 58)
                        .scaleEffect(x: isRolling ? 1.38 : 1.0, y: isRolling ? 0.86 : 1.0)
                        .matchedGeometryEffect(id: "tabDroplet", in: dropletNS)
                        .zIndex(0)
                }
                VStack(spacing: 2) {
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: selected ? 21 : 19, weight: .semibold))
                        .foregroundColor(selected ? systemBlue : inactive)
                        .scaleEffect(selected ? 1.08 : 1.0)
                        .frame(width: 44, height: 28)
                    Text(titles[i])
                        .font(.system(size: 10, weight: selected ? .semibold : .medium))
                        .foregroundColor(selected ? systemBlue : inactive)
                        .lineLimit(1)
                        .minimumScaleFactor(0.75)
                }
                .zIndex(1)
            }
            .frame(maxWidth: .infinity)
            .frame(height: 62)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

/// Duo 竖栏：纵向滚动玻璃胶囊（行程中纵向拉伸）。
struct LiquidGlassTabRail: View {
    @Binding var page: Int
    var titles: [String]
    var icons: [String]
    var fillIcons: [String]
    var systemEdge: CGFloat
    var onSelect: (Int) -> Void

    @Namespace private var dropletNS
    @State private var isRolling = false
    private let systemBlue = Color(red: 0.10, green: 0.48, blue: 1.0)
    private let inactive = Color.black.opacity(0.88)

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

    private func selectTab(_ i: Int) {
        guard i != page else { return }
        isRolling = true
        withAnimation(.spring(response: 0.42, dampingFraction: 0.62)) {
            onSelect(i)
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.28) {
            withAnimation(.spring(response: 0.32, dampingFraction: 0.78)) {
                isRolling = false
            }
        }
    }

    private func railItem(_ i: Int) -> some View {
        let selected = page == i
        return Button {
            selectTab(i)
        } label: {
            ZStack {
                if selected {
                    LiquidGlassDroplet(width: 56, height: 54)
                        .scaleEffect(x: isRolling ? 0.88 : 1.0, y: isRolling ? 1.32 : 1.0)
                        .matchedGeometryEffect(id: "railDroplet", in: dropletNS)
                }
                VStack(spacing: 2) {
                    Image(systemName: selected ? fillIcons[i] : icons[i])
                        .font(.system(size: selected ? 20 : 18, weight: .semibold))
                        .foregroundColor(selected ? systemBlue : inactive)
                        .scaleEffect(selected ? 1.08 : 1.0)
                        .frame(width: 44, height: 28)
                    Text(titles[i])
                        .font(.system(size: 10, weight: selected ? .semibold : .medium))
                        .foregroundColor(selected ? systemBlue : inactive)
                        .lineLimit(1)
                        .minimumScaleFactor(0.6)
                }
                .zIndex(1)
            }
            .frame(width: 58, height: 58)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
