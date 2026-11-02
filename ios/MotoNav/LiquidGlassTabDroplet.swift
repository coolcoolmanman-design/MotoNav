import SwiftUI

/// Travel-axis stretch while the selection morphs; settles with spring.
struct LiquidTabStretch: GeometryEffect {
    var stretch: CGFloat
    var horizontal: Bool

    var animatableData: CGFloat {
        get { stretch }
        set { stretch = newValue }
    }

    func effectValue(size: CGSize) -> ProjectionTransform {
        let sx = horizontal ? stretch : max(0.82, 2.0 - stretch)
        let sy = horizontal ? max(0.82, 2.0 - stretch) : stretch
        let t = CGAffineTransform(translationX: size.width / 2, y: size.height / 2)
            .scaledBy(x: sx, y: sy)
            .translatedBy(x: -size.width / 2, y: -size.height / 2)
        return ProjectionTransform(t)
    }
}

/// Soft blue specular that drifts across the selected glass (not neon bars).
///
/// `flow` 由拖动位移驱动（−1…1），静止时为 0。
/// 旧的 `repeatForever` 自转已去掉：它会让高光一直跑，既抢注意力也一直占渲染。
struct LiquidBlueHighlight: View {
    var width: CGFloat
    var height: CGFloat
    var flow: CGFloat = 0

    var body: some View {
        Capsule()
            .fill(
                LinearGradient(
                    colors: [
                        Color.clear,
                        Color(red: 0.35, green: 0.72, blue: 1.0).opacity(0.55),
                        Color.white.opacity(0.35),
                        Color.clear
                    ],
                    startPoint: .leading,
                    endPoint: .trailing
                )
            )
            .frame(width: width * 0.42, height: height * 0.55)
            .blur(radius: 6)
            .offset(x: flow * width * 0.30)
            .opacity(0.72 + min(0.22, abs(flow) * 0.22))
            .allowsHitTesting(false)
    }
}

/// 苹果 Music / Store 选中态回退：可滚动透明玻璃胶囊（iOS 15…25）。
struct LiquidGlassDroplet: View {
    var width: CGFloat = 64
    var height: CGFloat = 56

    var body: some View {
        let r = min(width, height) * 0.42
        ZStack {
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
            RoundedRectangle(cornerRadius: r, style: .continuous)
                .stroke(
                    LinearGradient(
                        colors: [Color.white.opacity(0.95), Color.white.opacity(0.38), Color.white.opacity(0.72)],
                        startPoint: .topLeading, endPoint: .bottomTrailing
                    ),
                    lineWidth: 1.6
                )
            RoundedRectangle(cornerRadius: r * 0.85, style: .continuous)
                .stroke(Color.white.opacity(0.42), lineWidth: 1.0)
                .padding(max(4, min(width, height) * 0.10))
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
            RoundedRectangle(cornerRadius: r, style: .continuous)
                .stroke(Color.white.opacity(0.28), lineWidth: 5)
                .blur(radius: 2.8)
                .opacity(0.65)
            Capsule()
                .fill(LinearGradient(colors: [Color.white.opacity(0.98), Color.white.opacity(0.55), Color.clear],
                                     startPoint: .leading, endPoint: .trailing))
                .frame(width: width * 0.48, height: max(4, height * 0.08))
                .offset(y: -height * 0.28)
                .blur(radius: 0.35)
            LiquidBlueHighlight(width: width, height: height)
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

/// 底栏：真机 Tabs = 导航 / 轨迹 / 圆屏 / 我的。
///
/// iOS 26+：底栏本体与选中胶囊是**同一 `GlassEffectContainer` 内的两块玻璃**，
/// 选中胶囊用 `.glassEffectID` 与容器关联 —— 玻璃形状的融合交给系统渲染，
/// 位移由 `progress`（连续值 0…n−1）驱动，而不是整格跳。
/// 按住可横向拖动：玻璃跟手、按拖速横向拉伸、途经按钮时与底栏玻璃交融；松手吸附回弹。
/// iOS 15…25：同一套手势与吸附，玻璃降级为 `LiquidGlassDroplet`。
struct LiquidGlassTabBar: View {
    @Binding var page: Int
    var titles: [String]
    var icons: [String]
    var fillIcons: [String]
    var onSelect: (Int) -> Void

    @Namespace private var glassSpace
    @Namespace private var dropletSpace
    @State private var progress: CGFloat = 0
    @State private var stretch: CGFloat = 1
    @State private var flow: CGFloat = 0
    @State private var dragging = false
    @State private var startProgress: CGFloat = 0

    private let settle = Animation.spring(response: 0.44, dampingFraction: 0.72, blendDuration: 0.16)
    private let inset: CGFloat = 6
    private let gap: CGFloat = 3
    private let capH: CGFloat = 58
    private let systemBlue = Color(red: 0.10, green: 0.48, blue: 1.0)

    private var activeIndex: Int {
        min(max(Int(progress.rounded()), 0), max(titles.count - 1, 0))
    }

    var body: some View {
        GeometryReader { geo in
            let count = max(titles.count, 1)
            let slot = max(1, (geo.size.width - inset * 2 - gap * CGFloat(count - 1)) / CGFloat(count))
            if #available(iOS 26, *) {
                officialBar
                    .gesture(dragGesture(travel: slot + gap))
            } else {
                fallbackBar
                    .gesture(dragGesture(travel: slot + gap))
            }
        }
        .frame(height: capH + inset * 2)
        .padding(.horizontal, 16)
        .padding(.bottom, 10)
        .onAppear { progress = CGFloat(page) }
        .onChange(of: page) { now in
            guard !dragging else { return }
            withAnimation(settle) { progress = CGFloat(now) }
        }
    }

    /// 与苹果示例一致：选中玻璃放在标签的 background 内，图标和文字永远位于玻璃上层。
    /// 0.16.2.7：底栏本体也进 `GlassEffectContainer`（原生 `.glassEffect`），
    /// 与选中透镜**同一容器**——两块玻璃靠近时由系统合成器融合，这才是参考视频的核心观感。
    /// 标签仍在玻璃上层（chrome 在 HStack 的 background 层），不会复现 0.16.2.4 的"玻璃吞标签"。
    @available(iOS 26, *)
    private var officialBar: some View {
        GlassEffectContainer(spacing: 40) {
            HStack(spacing: gap) {
                ForEach(0..<titles.count, id: \.self) { i in
                    tabLabel(i, whiteWhenSelected: true)
                        .background {
                            if activeIndex == i {
                                Capsule()
                                    .fill(.clear)
                                    .glassEffect(.regular.tint(.blue.opacity(0.38)).interactive(), in: Capsule())
                                    .glassEffectID("liquid-selection", in: glassSpace)
                                    .scaleEffect(x: stretch, y: 1, anchor: .center)
                                    .overlay {
                                        LiquidBlueHighlight(width: 68, height: capH, flow: flow)
                                            .opacity(0.38)
                                    }
                            }
                        }
                        .contentShape(Rectangle())
                }
            }
            .padding(inset)
            .background {
                Capsule()
                    .fill(.clear)
                    .glassEffect(.regular.tint(.white.opacity(0.06)), in: Capsule())
            }
        }
        .contentShape(Capsule())
    }

    private var fallbackBar: some View {
        HStack(spacing: gap) {
            ForEach(0..<titles.count, id: \.self) { i in
                tabLabel(i, whiteWhenSelected: false)
                    .background {
                        if activeIndex == i {
                            LiquidGlassDroplet(width: 68, height: capH)
                                .matchedGeometryEffect(id: "tab-droplet", in: dropletSpace)
                                .scaleEffect(x: stretch, y: 1, anchor: .center)
                        }
                    }
                    .contentShape(Rectangle())
            }
        }
        .padding(inset)
        .background { chrome }
        .contentShape(Capsule())
    }

    private func tabLabel(_ i: Int, whiteWhenSelected: Bool) -> some View {
        let selected = activeIndex == i
        let foreground: Color = selected ? (whiteWhenSelected ? .white : systemBlue) : .primary.opacity(0.68)
        return VStack(spacing: 5) {
            Image(systemName: selected ? fillIcons[i] : icons[i])
                .font(.system(size: 20, weight: .semibold))
            Text(titles[i])
                .font(.system(size: 10, weight: selected ? .semibold : .medium))
                .lineLimit(1)
                .minimumScaleFactor(0.75)
        }
        .foregroundStyle(foreground)
        .frame(maxWidth: .infinity)
        .frame(height: capH)
        .accessibilityLabel(Text(titles[i]))
    }

    private var chrome: some View {
        Capsule()
            .fill(.ultraThinMaterial)
            .overlay(Capsule().fill(Color.black.opacity(0.10)))
            .overlay(Capsule().stroke(Color.white.opacity(0.34), lineWidth: 0.7))
            .shadow(color: Color.black.opacity(0.12), radius: 16, y: 6)
    }

    /// 点击与拖动共用一个手势；选中玻璃始终留在标签背景层。
    private func dragGesture(travel: CGFloat) -> some Gesture {
        DragGesture(minimumDistance: 0, coordinateSpace: .local)
            .onChanged { value in
                if !dragging {
                    dragging = true
                    startProgress = progress
                }
                let count = CGFloat(max(titles.count, 1))
                let raw = startProgress + value.translation.width / max(1, travel)
                let overshoot = (value.predictedEndTranslation.width - value.translation.width) / max(1, travel)
                var transaction = Transaction()
                transaction.animation = nil
                withTransaction(transaction) {
                    progress = min(max(raw, 0), count - 1)
                    stretch = 1 + min(0.32, abs(overshoot) * 0.24)
                    flow = max(-1, min(1, value.translation.width / 56))
                }
            }
            .onEnded { value in
                let count = max(titles.count, 1)
                let target: Int
                if abs(value.translation.width) < 8 {
                    let index = (value.startLocation.x - inset) / max(1, travel)
                    target = min(max(Int(index.rounded(.down)), 0), count - 1)
                } else {
                    target = min(max(Int(progress.rounded()), 0), count - 1)
                }
                dragging = false
                withAnimation(settle) {
                    progress = CGFloat(target)
                    stretch = 1
                    flow = 0
                }
                if target != page { onSelect(target) }
            }
    }
}

/// Duo 竖栏：纵向滚动玻璃胶囊。
struct LiquidGlassTabRail: View {
    @Binding var page: Int
    var titles: [String]
    var icons: [String]
    var fillIcons: [String]
    var systemEdge: CGFloat
    var onSelect: (Int) -> Void

    @Namespace private var glassSpace
    @Namespace private var dropletNS
    @State private var stretch: CGFloat = 1.0
    private let systemBlue = Color(red: 0.10, green: 0.48, blue: 1.0)
    private let inactive = Color.black.opacity(0.88)
    private let spring = Animation.spring(response: 0.48, dampingFraction: 0.72, blendDuration: 0.18)

    var body: some View {
        Group {
            if #available(iOS 26, *) {
                officialRail
            } else {
                fallbackRail
            }
        }
    }

    @available(iOS 26, *)
    private var officialRail: some View {
        GlassEffectContainer(spacing: 12) {
            VStack(spacing: 8) {
                Spacer(minLength: systemEdge > 0 ? 170 : 12)
                ForEach(0..<titles.count, id: \.self) { i in
                    Button { selectTab(i) } label: {
                        VStack(spacing: 2) {
                            Image(systemName: page == i ? fillIcons[i] : icons[i])
                                .font(.system(size: 18, weight: .medium))
                            Text(titles[i])
                                .font(.system(size: 10, weight: .medium))
                                .lineLimit(1)
                                .minimumScaleFactor(0.6)
                        }
                        .foregroundStyle(page == i ? Color.white : Color.white.opacity(0.65))
                        .frame(width: 56, height: 54)
                        .background {
                            if page == i {
                                Capsule()
                                    .fill(.clear)
                                    .glassEffect(.regular.tint(.blue.opacity(0.65)).interactive(), in: Capsule())
                                    .glassEffectID("liquid-selection-rail", in: glassSpace)
                                    .modifier(LiquidTabStretch(stretch: stretch, horizontal: false))
                                    .overlay { LiquidBlueHighlight(width: 56, height: 54) }
                            }
                        }
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                }
                Spacer(minLength: 12)
            }
            .padding(.vertical, 10)
            .frame(width: max(66, systemEdge))
            .background {
                // 0.16.2.7：竖栏底也用原生玻璃，与选中透镜同容器融合（同底部导航条）。
                Capsule()
                    .fill(.clear)
                    .glassEffect(.regular.tint(.black.opacity(0.22)), in: Capsule())
            }
        }
        .padding(.vertical, 8)
        .padding(.trailing, 6)
    }

    private var fallbackRail: some View {
        VStack(spacing: 8) {
            Spacer(minLength: systemEdge > 0 ? 170 : 12)
            ForEach(0..<titles.count, id: \.self) { i in
                fallbackItem(i)
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
        stretch = 1.32
        withAnimation(spring) { onSelect(i) }
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.22) {
            withAnimation(.spring(response: 0.32, dampingFraction: 0.78)) { stretch = 1.0 }
        }
    }

    private func fallbackItem(_ i: Int) -> some View {
        let selected = page == i
        return Button {
            selectTab(i)
        } label: {
            ZStack {
                if selected {
                    LiquidGlassDroplet(width: 56, height: 54)
                        .modifier(LiquidTabStretch(stretch: stretch, horizontal: false))
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
