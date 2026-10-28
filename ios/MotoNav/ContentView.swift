import SwiftUI
import CoreLocation
import AVFoundation
import UIKit

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 主界面：圆屏连接状态、搜索目的地 / 导入高德分享路线、选路线、开始导航、巡航、圆屏设置、Key 设置、调试信息
///
/// iOS v0.15.0 UI 适配（沿用 v0.13.9 方案 + 液态玻璃）：
/// - 自动白夜模式：语义色见 AppTheme.swift（跟随系统，「圆屏设置 → 外观」可强制）
/// - iPhone Duo 折叠屏：外屏导航 Tab 竖排在最右侧系统状态条里，展开 / 普通机型横排底部
/// - 导航 Tab 全部换成液态玻璃（iOS 26 glassEffect，旧系统降级毛玻璃）
struct ContentView: View {
    @ObservedObject var ble = BleLink.shared
    @ObservedObject var navi = NaviBridge.shared
    @ObservedObject var independent = WebMotorSession.shared
    @ObservedObject var search = SearchService.shared
    @ObservedObject var settings = Settings.shared
    @ObservedObject var weather = Weather.shared
    @ObservedObject var map = MapFeeder.shared
    @ObservedObject var history = History.shared
    @ObservedObject var favorites = Favorites.shared
    @State private var page = 0
    @State private var showWebMotor = false
    @State private var independentMode: IndependentMode = .motorcycle
    @State private var showRides = false
    @AppStorage("voice_rate") private var voiceRate = Double(AVSpeechUtteranceDefaultSpeechRate)
    @AppStorage("voice_pitch") private var voicePitch = 1.0
    @AppStorage("voice_id") private var voiceID = ""

    @State private var query = ""
    @State private var dest: SearchService.Place?
    @State private var showNavi = false
    @State private var showDebug = false
    @State private var showKeys = false                 // iOS v0.7 填写地图 Key 页面
    private static var keyPrompted = false              // 这次打开 App 已经提醒过填 key 了
    @State private var shareBusy = false
    @State private var shareStatus = ""
    @State private var cruiseNote = ""
    @State private var addingVia = false               // 正在搜途经点
    @Environment(\.horizontalSizeClass) private var hSizeClass

    // 语义色（自动白夜）：转发到 AppTheme 的 dynamic 色，
    // KeySetupView / RotationDial / Weather 等文件还引用这些旧名字，不用改就能跟随白夜切换
    static let bg = AppTheme.bg
    static let card = AppTheme.card
    static let stroke = AppTheme.stroke
    static let pink = AppTheme.pink
    static let cyan = AppTheme.cyan
    static let sub = AppTheme.sub

    var body: some View {
        GeometryReader { geo in
            let rail = TabLayout.useSideRail(size: geo.size, hSizeClass: hSizeClass, placement: settings.tabPlacement)
            let wide = TabLayout.isWide(size: geo.size, hSizeClass: hSizeClass)
            // 外屏上系统把右侧一条划成安全区（画时间 + 连接状态圆环），竖栏要伸进这条区域；
            // 内屏横屏走竖栏时窗口铺满物理屏，sysEdge 自然为 0
            let sysEdge = rail ? max(0, (TabLayout.sceneScreen.map { $0.nativeBounds.width / $0.nativeScale } ?? 0) - geo.size.width) : 0
            ZStack {
                if rail {
                    // 外屏 / 内屏横屏：内容在左，液态玻璃竖栏贴右侧（外屏时盖住系统状态条）
                    HStack(spacing: 0) {
                        VStack(spacing: 0) {
                            activeNavigationBanner
                            pages(wide: wide)
                        }
                        rightRail(systemEdge: sysEdge)
                    }
                    .ignoresSafeArea(.container, edges: .trailing)
                } else {
                    // 底部：v0.15.8 内容 edge-to-edge 穿透液态玻璃条后面（实时模糊），导航条悬浮；
                    // 旧布局是 VStack 上下排，玻璃条下方露出一条页面底色的实色填充栏，已移除
                    ZStack(alignment: .bottom) {
                        VStack(spacing: 0) {
                            activeNavigationBanner
                            pages(wide: wide)
                        }
                        .ignoresSafeArea(.container, edges: .bottom)
                        bottomBar
                    }
                }
            }
            .background(
                // 塞尔达40周年特别版：固定背景（暗金三角纹理 + 居中旗帜，已去水印）
                Image("Zelda40Background")
                    .resizable()
                    .scaledToFill()
                    .ignoresSafeArea()
            )
            .overlay(ZeldaSplash())
            .fullScreenCover(isPresented: $showWebMotor) { WebMotorView(mode:independentMode) }
            .sheet(isPresented: $showRides) { NavigationView { RideHistoryView() } }
            .fullScreenCover(isPresented: $showNavi) { NaviScreen() }
            .sheet(isPresented: $showKeys) { KeySetupView() }
            .onAppear {                                    // iOS v0.7 打开 App 时，高德或高德 key 没填就提醒一次
                if !ContentView.keyPrompted && settings.keysMissing {
                    ContentView.keyPrompted = true
                    showKeys = true
                }
            }
            .onOpenURL { url in                            // iOS v0.4 高德里“分享到 MotoNav”：motonav://share?t=分享的文字
                guard url.scheme == "motonav",
                      let t = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?.first(where: { $0.name == "t" })?.value
                else { return }
                importShare(t)
            }
        }
    }

    // MARK: 5 个页面（上游结构：5 页保活，切显隐；内屏展开时限宽居中，首页两栏利用宽度）
    private func pages(wide: Bool) -> some View {
        ZStack {
            ForEach(0..<5) { index in
                ScrollView {
                    Group {
                        if index == 0 && wide {
                            // 内屏（展开）：苹果 Duo 规范——利用宽度做两栏布局，左主右辅
                            HStack(alignment: .top, spacing: 16) {
                                VStack(alignment: .leading, spacing: 16) {
                                    header
                                    cruiseCard
                                    routeButtons
                                    destCard
                                }
                                VStack(alignment: .leading, spacing: 16) {
                                    statusCard
                                    favoritesCard
                                    GlassChip(title: "骑行轨迹", icon: "figure.outdoor.cycling", tint: Self.cyan) { showRides = true }
                                }
                            }
                        } else {
                            VStack(alignment: .leading, spacing: 16) {
                                header
                                if index == 0 {
                                    cruiseCard
                                    statusCard
                                    routeButtons
                                    destCard
                                    favoritesCard
                                    GlassChip(title: "骑行轨迹", icon: "figure.outdoor.cycling", tint: Self.cyan) { showRides = true }
                                } else if index == 1 { displayCard }
                                else if index == 2 { voiceCard }
                                else if index == 3 { statusCard; settingsCard; duoCard; debugCard }   // 塞尔达40周年特别版：外观卡移除（固定深色）
                                else { keyCard; Text(weather.detail).foregroundColor(Self.sub) }
                            }
                        }
                    // v0.15.8 内容现在穿透玻璃条到底缘：底部留白要盖住玻璃条（约 70pt）+ 底部安全区（34pt）
                    }.padding(.horizontal, 16).padding(.top, 16).padding(.bottom, 116)
                }
                .opacity(page == index ? 1 : 0).allowsHitTesting(page == index).accessibilityHidden(page != index)
            }
        }
        .frame(maxWidth: wide ? 1080 : .infinity)
        .frame(maxWidth: .infinity)
    }

    /// 摩托车 / 自行车路线规划入口（首页用）
    private var routeButtons: some View {
        HStack(spacing: 10) {
            GlassButton(title: "摩托车路线规划", style: .tinted, tint: Self.pink, compact: true) {
                if !independent.running { independent.arrived = false }
                independentMode = .motorcycle; showWebMotor = true
            }
            GlassButton(title: "自行车路线规划", style: .tinted, tint: Self.cyan, compact: true) {
                if !independent.running { independent.arrived = false }
                independentMode = .bicycle; showWebMotor = true
            }
        }
    }

    private var activeNavigationBanner: some View {
        Group {
            if navi.navigating || independent.running {
                HStack {
                    VStack(alignment:.leading,spacing:3) {
                        Text("导航继续运行中").bold()
                        Text(independent.running ? independent.instruction : "\(navi.uiDir) · \(navi.uiDist) 米")
                            .font(.caption).lineLimit(1)
                    }
                    Spacer()
                    GlassChip(title: "回到导航", icon: "arrow.triangle.turn.up.right.circle", tint: Self.pink) {
                        if independent.running { showWebMotor = true } else { showNavi = true }
                    }
                }.padding(12).background(Self.card).foregroundColor(AppTheme.text)
            }
        }
    }

    // MARK: 标题
    private var header: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text("MotoNav")
                .font(.system(size: 34, weight: .heavy, design: .rounded))
                .italic()
                .foregroundStyle(LinearGradient(colors: [ContentView.pink, ContentView.cyan], startPoint: .leading, endPoint: .trailing))
            Text("圆屏导航 · \(APP_VERSION)").font(.caption).foregroundColor(ContentView.sub)
        }
        .padding(.top, 8)
    }

    // MARK: 圆屏状态
    private var statusCard: some View {
        CardView {
            HStack {
                Circle().fill(ble.isReady ? Color.green : Color.orange).frame(width: 10, height: 10)
                Text("圆屏").foregroundColor(AppTheme.text).bold()
                Spacer()
                GlassChip(title: ble.isReady ? "断开" : "连接",
                          icon: ble.isReady ? "bolt.slash" : "bolt.fill",
                          tint: ble.isReady ? nil : Self.cyan) {
                    ble.isReady ? ble.disconnect() : ble.connect()
                }
            }
            Text(ble.status).font(.footnote).foregroundColor(ContentView.sub)
            // iOS v0.6 圆屏画小地图时每 2 秒回报一次帧率
            if ble.isReady, Date().timeIntervalSince(ble.statAt) < 10, !ble.screenFps.isEmpty {
                Text("小地图 \(ble.screenFps) 帧/秒").font(.footnote).foregroundColor(ContentView.sub)
            }
            if navi.navigating {
                Text("导航中：\(navi.uiDir) \(navi.uiDist) 米 · \(navi.uiSpeed) km/h").font(.footnote).foregroundColor(ContentView.cyan)
                if navi.uiRemainM > 0 {
                    Text("剩余 \(NaviBridge.distText(navi.uiRemainM)) · 约 \(NaviBridge.durText(navi.uiRemainS)) · \(ContentView.etaText(navi.uiRemainS)) 到达")
                        .font(.footnote).foregroundColor(AppTheme.text)
                }
                GlassChip(title: "回到导航画面", icon: "arrow.triangle.turn.up.right.circle", tint: ContentView.pink, compact: true) { showNavi = true }
            }
            Text("小地图：\(map.shortStatus) · 高德 · \(Settings.themeName(settings.theme))")
                .font(.caption).foregroundColor(ContentView.sub)
        }
    }

    // MARK: 去哪儿
    private var destCard: some View {
        CardView {
            Text("去哪儿").foregroundColor(AppTheme.text).bold()
            HStack(spacing: 10) {
                // 只在按键盘上的“搜索”或右边按钮时才查高德（边打字边查太费额度）
                TextField(addingVia ? "搜索途经点（比如加油站）" : "搜索地点、地址", text: $query)
                    .textFieldStyle(.plain)
                    .padding(12)
                    .background(RoundedRectangle(cornerRadius: 12).fill(ContentView.bg))
                    .foregroundColor(AppTheme.text)
                    .submitLabel(.search)
                    .onSubmit { if !needAmapKey() { search.search(query) } }
                    .onChange(of: query) { if $0.isEmpty { search.clear() } }
                GlassButton(title: "搜索", icon: "magnifyingglass", style: .tinted, tint: Self.pink, compact: true) {
                    hideKeyboard(); if !needAmapKey() { search.search(query) }
                }
            }
            // iOS v0.3 导入高德 App 分享的路线（比如高德里规划好的摩托车路线）
            if !addingVia {
            GlassChip(title: shareBusy ? "正在读取分享的路线…" : "粘贴高德分享的路线", icon: "doc.on.clipboard", tint: AppTheme.orange) { pasteShare() }
            .disabled(shareBusy)
            Text("在高德里规划好路线 → 分享 → 选 MotoNav；或者复制链接后点上面这行").font(.caption2).foregroundColor(ContentView.sub)
            }
            if !shareStatus.isEmpty { Text(shareStatus).font(.footnote).foregroundColor(ContentView.sub) }
            if !search.status.isEmpty { Text(search.status).font(.footnote).foregroundColor(ContentView.sub) }
            ForEach(search.results.prefix(8)) { p in
                Button { choose(p) } label: {
                    HStack(alignment: .center, spacing: 8) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(p.name).foregroundColor(AppTheme.text)
                            Text(p.address).font(.caption).foregroundColor(ContentView.sub).lineLimit(1)
                        }
                        Spacer(minLength: 4)
                        if let d = distanceTo(p) { Text(d).font(.caption).foregroundColor(ContentView.cyan) }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.vertical, 6)
                }
                Divider().background(ContentView.stroke)
            }
            // iOS v0.4 最近去过（还没选目的地、或者正在选途经点时显示）
            if search.results.isEmpty && (dest == nil || addingVia) && !history.places.isEmpty {
                Text(addingVia ? "从最近去过里选途经点" : "最近去过").font(.caption).foregroundColor(ContentView.sub).padding(.top, 4)
                ForEach(history.places.prefix(8)) { p in
                    Button { choose(p) } label: {
                        HStack(alignment: .center, spacing: 8) {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(p.name).foregroundColor(AppTheme.text)
                                if !p.address.isEmpty { Text(p.address).font(.caption).foregroundColor(ContentView.sub).lineLimit(1) }
                            }
                            Spacer(minLength: 4)
                            if let d = distanceTo(p) { Text(d).font(.caption).foregroundColor(ContentView.cyan) }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.vertical, 4)
                    }
                    Divider().background(ContentView.stroke)
                }
                GlassChip(title: "清除记录", foreground: ContentView.sub, compact: true) { history.clear() }
            }
            if let d = dest {
                HStack(spacing: 8) {
                    Text("目的地：\(d.name)").font(.footnote).foregroundColor(AppTheme.text)
                    GlassChip(title: "收藏", icon: "star", tint: Self.cyan, compact: true) { favorites.add(d) }
                    Spacer()
                    if !navi.navigating {
                        GlassChip(title: "换目的地", tint: ContentView.cyan, foreground: ContentView.cyan, compact: true) {
                            dest = nil; query = ""; addingVia = false; shareStatus = ""
                            search.clear()
                            navi.clearPlan()
                        }
                    }
                }
                if let sr = navi.shareRoute {
                    Text("按高德分享的\(sr.kind)路线走（原路线 \(NaviBridge.distText(Int(sr.total)))）").font(.caption).foregroundColor(Color.orange)
                    if !navi.shareMatch.isEmpty { Text(navi.shareMatch).font(.caption).foregroundColor(ContentView.sub) }
                }
                if navi.shareRoute == nil {                 // 途经点（分享的路线自带途经点）
                    HStack(spacing: 12) {
                        Text(navi.vias.isEmpty ? "途经：无" : "途经：" + navi.vias.map { $0.name }.joined(separator: " → "))
                            .font(.caption).foregroundColor(ContentView.sub).lineLimit(2)
                        Spacer()
                        if addingVia {
                            GlassChip(title: "取消", foreground: ContentView.sub, compact: true) { addingVia = false; query = d.name; search.clear() }
                        } else {
                            GlassChip(title: "+ 途经点", tint: ContentView.cyan, foreground: ContentView.cyan, enabled: navi.vias.count < 3, compact: true) { addingVia = true; query = ""; search.clear() }
                            if !navi.vias.isEmpty {
                                GlassChip(title: "清除", foreground: ContentView.sub, compact: true) { navi.clearVias() }
                            }
                        }
                    }
                    if addingVia {
                        Text("在上面搜索途经点（比如加油站），或者从最近去过里选；最多 3 个，按顺序经过")
                            .font(.caption2).foregroundColor(Color.orange)
                    }
                }
                Toggle("不走高速", isOn: $settings.avoidHighway)
                    .foregroundColor(AppTheme.text).tint(ContentView.pink)
                    .onChange(of: settings.avoidHighway) { _ in navi.replan() }
                if navi.calculating || navi.routeOptions.isEmpty {
                    Text(navi.calculating ? "正在规划路线…" : navi.info).font(.footnote).foregroundColor(ContentView.sub)
                }
                if navi.planFailed && !navi.calculating {   // iOS v0.6 规划失败 / 超时
                    GlassChip(title: "重新规划", icon: "arrow.clockwise", tint: ContentView.cyan) { navi.replan() }
                }
                if !navi.routeOptions.isEmpty {
                    RouteMapView(routes: navi.routeOptions, selected: navi.selectedRouteID,
                                 ghost: navi.shareRoute?.pts.map { CLLocationCoordinate2D(latitude: $0.lat, longitude: $0.lng) } ?? [],
                                 dest: navi.destCoord,
                                 vias: navi.vias.map { CLLocationCoordinate2D(latitude: $0.lat, longitude: $0.lng) },
                                 night: settings.paletteNight)
                        .frame(height: 230)
                        .clipShape(RoundedRectangle(cornerRadius: 14))
                    HStack(spacing: 8) {
                        ForEach(navi.routeOptions) { o in
                            RouteCard(option: o, selected: o.id == navi.selectedRouteID)
                                .onTapGesture { navi.select(routeID: o.id) }
                        }
                    }
                }
                HStack(spacing: 12) {
                    GlassButton(title: "开始导航", icon: "location.fill", style: .prominent, tint: ContentView.pink, enabled: navi.routeReady) {
                        navi.start(emulate: false); showNavi = true
                    }
                    GlassButton(title: "模拟导航", icon: "play.fill", style: .tinted, tint: AppTheme.purple, enabled: navi.routeReady) {
                        navi.start(emulate: true); showNavi = true
                    }
                }
            }
        }
    }

    /// 离现在位置的直线距离（搜索结果、最近去过右边显示）
    private func distanceTo(_ p: SearchService.Place) -> String? {
        guard let me = navi.myPosition() else { return nil }
        let d = Int(NaviBridge.distM(me.lat, me.lng, p.lat, p.lng))
        return d >= 1000 ? String(format: "%.1f 公里", Double(d) / 1000) : "\(d) 米"
    }

    /// 剩余秒数 → 到达时间“18:52”
    static func etaText(_ remainS: Int) -> String {
        let f = DateFormatter(); f.dateFormat = "HH:mm"
        return f.string(from: Date().addingTimeInterval(TimeInterval(remainS)))
    }

    /// 点了搜索结果或最近去过：选目的地，或者（正在选途经点时）加一个途经点
    private func choose(_ p: SearchService.Place) {
        guard !navi.navigating else {shareStatus="导航仍在运行；如需更换目的地，请先结束导航";return}
        guard !WebMotorSession.shared.running else { shareStatus = "请先结束网页导航"; return }
        search.clear()
        hideKeyboard()
        if addingVia {
            addingVia = false
            query = dest?.name ?? ""
            navi.addVia(name: p.name, lat: p.lat, lng: p.lng)
            return
        }
        dest = p
        query = p.name                                  // 只改文字，不会再触发搜索
        shareStatus = ""
        History.shared.add(p)
        if needAmapKey() { return }
        navi.plan(toLat: p.lat, lng: p.lng)
    }

    /// 读剪贴板里的高德分享链接，还原路线后按它规划
    private func pasteShare() {
        guard let t = UIPasteboard.general.string, AmapShare.findUrl(t) != nil else {
            shareStatus = "剪贴板里没有高德的分享链接：先在高德里打开路线 → 分享 → 复制链接"
            return
        }
        importShare(t)
    }

    /// 导入高德分享的路线（剪贴板粘贴，或者从高德“分享到 MotoNav”过来）
    private func importShare(_ t: String) {
        guard AmapShare.findUrl(t) != nil else { shareStatus = "分享的内容里没有高德链接"; return }
        if navi.navigating || WebMotorSession.shared.running { shareStatus = "正在导航：先退出导航，再导入分享的路线"; return }
        addingVia = false
        hideKeyboard()
        shareBusy = true
        shareStatus = "正在读取高德分享的路线…"
        DispatchQueue.global(qos: .userInitiated).async {
            let res = Result { try AmapShare.load(t) }
            DispatchQueue.main.async {
                shareBusy = false
                guard !navi.navigating, !WebMotorSession.shared.running else { shareStatus = "导航已开始，请结束后重新导入"; return }
                switch res {
                case .success(let r):
                    shareStatus = "已读取：\(r.kind)路线 \(NaviBridge.distText(r.distance > 0 ? r.distance : Int(r.total)))" +
                        (r.timeS > 0 ? "，约 \(NaviBridge.durText(r.timeS))" : "") +
                        (r.restored ? "" : "（高德没能原样还原，可能和分享的稍有不同）")
                    let name = r.endName.isEmpty ? "分享的目的地" : r.endName
                    dest = SearchService.Place(name: name, address: "高德分享的\(r.kind)路线", lat: r.endLat, lng: r.endLng)
                    History.shared.add(SearchService.Place(name: name, address: "", lat: r.endLat, lng: r.endLng))
                    query = name
                    search.clear()
                    if r.kind == "自行车" {
                        navi.clearPlan()
                        WebMotorSession.shared.route = MotorRoutes.fromShare(r)
                        independentMode = .bicycle
                        showWebMotor = true
                    } else if !needAmapKey() { navi.plan(toLat: r.endLat, lng: r.endLng, share: r) }
                case .failure(let e):
                    shareStatus = "读取失败：\(e.localizedDescription)"
                }
            }
        }
    }

    // MARK: 巡航
    private var cruiseCard: some View {
        CardView {
            HStack {
                Text("巡航模式").foregroundColor(AppTheme.text).bold()
                Spacer()
                if navi.cruising { Text("进行中 · \(navi.uiSpeed) km/h").font(.caption).foregroundColor(ContentView.cyan) }
            }
            Text("不设目的地：骑车时提醒前方测速、电子眼和急弯、学校、事故多发等道路安全提示，圆屏照常显示小地图")
                .font(.caption).foregroundColor(ContentView.sub)
            if navi.cruising { Text(navi.info).font(.footnote).foregroundColor(ContentView.sub) }
            GlassButton(title: navi.cruising ? "结束巡航" : "开始巡航",
                        icon: navi.cruising ? "stop.fill" : "sailcircle",
                        style: navi.cruising ? .clear : .tinted,
                        tint: ContentView.cyan,
                        enabled: !navi.navigating && !WebMotorSession.shared.running) {
                cruiseNote = ""
                if navi.cruising { navi.stopCruise() }
                else if !needAmapKey(), let err = navi.startCruise() { cruiseNote = err }
            }
            if !cruiseNote.isEmpty { Text(cruiseNote).font(.caption).foregroundColor(Color.orange) }
        }
    }

    // MARK: 圆屏设置
    private var settingsCard: some View {
        CardView {
            Text("圆屏设置").foregroundColor(AppTheme.text).bold()
            Picker("开机动画", selection: $settings.bootBrand) {
                ForEach(Settings.bootBrands.indices, id: \.self) { i in Text(Settings.bootBrands[i]).tag(i) }
            }.tint(ContentView.cyan)
            Text(settings.bootStatus).font(.caption).foregroundColor(ContentView.sub)
            Toggle("蓝牙媒体按键（QQ 音乐等）", isOn: $settings.musicHID).foregroundColor(AppTheme.text).tint(ContentView.pink)
            if settings.musicHID {
                Text(ble.mediaHIDSupported
                     ? (ble.mediaHIDReady && ble.mediaHIDMode ? "媒体按键已就绪" : ble.mediaAMSControls > 0 && ble.mediaHIDMode ? "AMS 媒体控制已就绪，HID 当前不可用" : "请在 iPhone 设置 → 蓝牙中配对 MotoNav；按圆屏配对码提示完成")
                     : "需要圆屏 HID 固件 v7.9.0 或更新版，连接后自动同步")
                    .font(.caption).foregroundColor(ContentView.sub)
                if !ble.mediaSongInfo.isEmpty {
                    Text(ble.mediaSongInfo).font(.caption).foregroundColor(ContentView.cyan)
                }
                Toggle("显示逐句歌词", isOn: $settings.musicLyrics).foregroundColor(AppTheme.text).tint(ContentView.pink)
                Text("默认关闭，上方歌名、下方歌手。开启后按歌曲信息从 LRCLIB 匹配歌词，歌词显示在歌手下方；未匹配到时保留歌名和歌手。")
                    .font(.caption).foregroundColor(ContentView.sub)
                if !settings.musicLyrics && ble.mediaLyricsActive { Text("正在关闭圆屏歌词…").font(.caption).foregroundColor(ContentView.sub) }
                if settings.musicLyrics {
                    Text(ble.mediaLyricsSupported ? ble.mediaLyricsStatus : "需要圆屏固件 v7.11.0 或更新版")
                        .font(.caption).foregroundColor(ContentView.cyan)
                }
                Text("先在网易云或 QQ 音乐开始播放，再用圆屏控制。新固件通过 iPhone AMS 显示歌名、歌手和状态，信息取决于当前播放器提供的内容。HID SENT 表示按键已发送。")
                    .font(.caption).foregroundColor(ContentView.sub)
            } else {
                GlassChip(title: "授权 Apple Music 圆屏控制", icon: "music.note", tint: ContentView.cyan) { MusicBridge.shared.authorize() }
            }
            Text("关闭蓝牙媒体按键后，改用系统音乐 App 控制。不读取来电号码。巡航连续停车 10 分钟自动结束。")
                .font(.caption).foregroundColor(ContentView.sub)
            Text("圆屏方向").foregroundColor(AppTheme.text).padding(.top, 6)
            RotationDial(degrees: $settings.rotationDeg)      // iOS v0.5.1 按住圆盘转动调角度
            Text("按住圆盘转动（顺时针为正），粉色指针是画面上方在圆屏上朝的方向，圆屏会跟着实时转。转到 0°/90°/180°/270° 附近会吸住。任意角度要圆屏固件 v7.10.1；旧固件会转到最接近的 90°")
                .font(.caption2).foregroundColor(ContentView.sub)
            // iOS v0.6 屏幕刷新速度、快速刷屏（圆屏存起来并自动重启；当前值来自圆屏的帧率回报）
            Text("刷新速度").foregroundColor(AppTheme.text).padding(.top, 6)
            GlassSegmented(options: [(40, "40 标准"), (60, "60"), (80, "80")],
                           selection: Binding(get: { ble.screenMHz }, set: { ble.setScreenMHz($0) }))
                .disabled(!ble.isReady)
            Toggle("快速刷屏", isOn: Binding(get: { ble.screenFast == 1 }, set: { ble.setScreenFast($0) }))
                .foregroundColor(AppTheme.text).tint(ContentView.pink)
                .disabled(!ble.isReady)
            Text(ble.screenMHz < 0
                 ? "越快画面撕裂越轻；花屏就切回 40、关掉快速刷屏。切换后圆屏会自动重启。现在的值要等圆屏显示过小地图才知道"
                 : "越快画面撕裂越轻；花屏就切回 40、关掉快速刷屏。切换后圆屏会自动重启")
                .font(.caption2).foregroundColor(ContentView.sub)
            Toggle("亮度跟着昼夜自动调", isOn: $settings.brightAuto).foregroundColor(AppTheme.text).tint(ContentView.pink)
            if settings.brightAuto {
                HStack {
                    Text("白天").foregroundColor(AppTheme.text)
                    Slider(value: $settings.brightDay, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightDay))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
                HStack {
                    Text("夜间").foregroundColor(AppTheme.text)
                    Slider(value: $settings.brightNight, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightNight))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
                if !settings.brightDesc.isEmpty { Text(settings.brightDesc).font(.caption2).foregroundColor(ContentView.sub) }
            } else {
                HStack {
                    Text("亮度").foregroundColor(AppTheme.text)
                    Slider(value: $settings.brightness, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightness))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
            }
        }
    }

    private var displayCard: some View {
        CardView {
            Text("圆屏显示").foregroundColor(AppTheme.text).bold()
            Toggle("路程进度环", isOn: $settings.progressRing).foregroundColor(AppTheme.text).tint(Self.pink)
            GlassSegmented(options: [(0, "关闭"), (300, "0.3秒"), (600, "0.6秒")],
                           selection: $settings.leadMs)
            Text("地图状态：\(map.shortStatus)").font(.caption).foregroundColor(Self.sub)
            HStack {
                Text("风格").foregroundColor(AppTheme.text)
                Spacer()
                Picker("风格", selection: $settings.theme) {
                    ForEach(Settings.themeOrder, id: \.self) { t in Text(Settings.themeName(t)).tag(t) }
                }
                .tint(ContentView.cyan)
            }
            Text(Settings.themeDesc(settings.theme)).font(.caption2).foregroundColor(ContentView.sub)
            Toggle("小地图", isOn: $settings.mapOn).foregroundColor(AppTheme.text).tint(ContentView.pink)
            if settings.mapOn {
                Text("地图来源：高德（普通底图和卫星图）").font(.caption).foregroundColor(ContentView.sub)
                if settings.mapSource == MapFeeder.SRC_AMAP && settings.theme == 1 {
                    Toggle("俯视建筑轮廓", isOn: $settings.buildings).foregroundColor(AppTheme.text).tint(ContentView.pink)
                    Text(ble.buildingsSupported ? "圆屏支持建筑图层，默认提取高德建筑俯视轮廓" : "正在查询圆屏建筑能力；确认前先显示普通底图")
                        .font(.caption2).foregroundColor(ContentView.sub)
                    if ble.isReady && !ble.buildingsSupported {
                        GlassChip(title: "重新查询建筑能力", foreground: ContentView.cyan, compact: true) { ble.retryBuildingCapability() }
                    }
                    if !map.buildingInfo.isEmpty { Text(map.buildingInfo).font(.caption).foregroundColor(ContentView.sub) }
                }
                Text(map.offlineStatus).font(.caption).foregroundColor(ContentView.sub)
                Text("选定路线后，前台自动下载沿途地图；锁屏使用已保存地图包。请等下载完成再出发。")
                    .font(.caption2).foregroundColor(ContentView.sub)
                Text("走过的路线").foregroundColor(AppTheme.text)
                GlassSegmented(options: [(1, "变灰"), (2, "擦除")], selection: $settings.trail)
                Toggle("清爽模式", isOn: $settings.clean).foregroundColor(AppTheme.text).tint(ContentView.pink)
                Text("清爽模式：地图保留，去掉红绿灯、电子眼、安全提示、测速牌和行程信息").font(.caption2).foregroundColor(ContentView.sub)
            }
            Text("夜间配色").foregroundColor(AppTheme.text)
            GlassSegmented(options: [(0, "自动（日出日落）"), (1, "白天"), (2, "夜间")],
                           selection: $settings.nightMode)
            if !settings.nightDesc.isEmpty { Text(settings.nightDesc).font(.caption2).foregroundColor(ContentView.sub) }
            Toggle("弯道急缓上色", isOn: $settings.curve).foregroundColor(AppTheme.text).tint(ContentView.pink)
        }
    }
    private var favoritesCard: some View {
        CardView {
            Text("常用目的地").foregroundColor(AppTheme.text).bold()
            if favorites.places.isEmpty { Text("选择目的地后点收藏").foregroundColor(Self.sub) }
            ForEach(favorites.places) { place in
                HStack {
                    Button(place.name) { choose(place.searchPlace) }.foregroundColor(Self.cyan)
                    Spacer()
                    GlassChip(title: "移除", foreground: ContentView.sub, compact: true) { favorites.remove(place) }
                }
            }
        }
    }
    private var voiceCard: some View {
        CardView {
            Text("导航语音").foregroundColor(AppTheme.text).bold()
            Toggle("导航静音", isOn: $settings.voiceMuted)
            Toggle("播报时压低音乐", isOn: $settings.duck)
            Picker("系统音色", selection: $voiceID) {
                Text("自动优选中文").tag("")
                ForEach(AVSpeechSynthesisVoice.speechVoices().filter { $0.language.hasPrefix("zh") }, id: \.identifier) { voice in
                    Text(voice.name).tag(voice.identifier)
                }
            }
            Text("语速"); Slider(value: $voiceRate, in: 0.3...0.6)
            Text("音调"); Slider(value: $voicePitch, in: 0.7...1.4)
            GlassChip(title: "试听播报", icon: "speaker.wave.2", tint: Self.pink) { Speech.shared.test() }
        }.foregroundColor(AppTheme.text).tint(Self.pink)
    }

    // MARK: Key 设置
    private var keyCard: some View {
        CardView {
            Text("Key 设置").foregroundColor(AppTheme.text).bold()
            Text("高德 Key：\(Settings.mask(settings.amapKeyInUse))（搜索、导航、巡航用）")
                .font(.footnote).foregroundColor(settings.amapKeyInUse.isEmpty ? Color.orange : ContentView.sub)
            Text("和风天气：\(weather.host.isEmpty || weather.key.isEmpty ? "未配置（可选）" : "已配置")")
                .font(.footnote).foregroundColor(ContentView.sub)
            GlassChip(title: settings.keysMissing ? "去填写" : "修改", icon: "key", tint: ContentView.cyan) { showKeys = true }
        }
    }

    /// 没填高德 key：弹出填写页，返回 true（调用的地方就不往下做了）
    private func needAmapKey() -> Bool {
        if settings.amapKeyInUse.isEmpty { hideKeyboard(); showKeys = true; return true }
        return false
    }

    // MARK: 调试信息
    private var debugCard: some View {
        CardView {
            GlassChip(title: showDebug ? "收起调试信息" : "调试信息", icon: "ladybug", tint: ContentView.cyan) { showDebug.toggle() }
            if showDebug {
                Text("""
                圆屏：\(ble.status)
                已发送 \(ble.sentCount) 条，最近：\(ble.lastSent)
                \(ble.screenStats)
                导航：\(navi.info)
                路名：\(navi.uiRoad)
                \(navi.laneRaw)
                电子眼：\(navi.camRaw)
                巡航：\(navi.cruiseRaw)
                \(navi.extraInfo)
                小地图：\(map.infoText)
                地图数据累计发送 \(ble.bulkKB) KB
                天气：\(weather.info)
                """)
                .font(.caption.monospaced())
                .foregroundColor(ContentView.sub)
                .textSelection(.enabled)
            }
        }
    }

    // MARK: 折叠屏适配（iPhone Duo，iOS v0.15.0）
    private var duoCard: some View {
        CardView {
            Text("折叠屏适配").foregroundColor(AppTheme.text).bold()
            GlassSegmented(
                options: TabPlacement.allCases.map { ($0.rawValue, $0.name) },
                selection: $settings.tabPlacement)
            Text("iPhone Duo 合盖用外屏时，导航条自动竖排到最右侧的时间/状态条里；展开竖屏横排在底部，展开横屏竖排在右侧（苹果 Duo 规范）。遇到识别不准可在这里手动固定。")
                .font(.caption2).foregroundColor(AppTheme.sub)
        }
    }

    // MARK: 底部导航条（液态玻璃胶囊，悬浮）
    private var bottomBar: some View {
        HStack(spacing: 2) {
            ForEach(0..<5, id: \.self) { i in tabButton(i) }
        }
        .padding(.horizontal, 6).padding(.vertical, 6)
        .liquidGlass(Capsule(), light: true)   // zelda40.7：白天强制毛玻璃（透 + 不发白），跳过原生采样
        .padding(.horizontal, 12).padding(.top, 6).padding(.bottom, 8)
        .environment(\.colorScheme, .light)    // 文字/图标取浅色语义（深字），适配透明白天玻璃
    }

    private func tabButton(_ i: Int) -> some View {
        let selected = page == i
        return Button {
            hideKeyboard()
            page = i
        } label: {
            VStack(spacing: 3) {
                Image(systemName: selected ? TabSpec.fillIcons[i] : TabSpec.icons[i])
                    .font(.system(size: 20, weight: .semibold))
                    .foregroundColor(selected ? AppTheme.tabAccents[i] : AppTheme.sub.opacity(0.72))
                    .frame(width: 44, height: 28)
                Text(TabSpec.titles[i])
                    .font(.system(size: 11, weight: selected ? .bold : .medium))
                    .foregroundColor(selected ? AppTheme.tabAccents[i] : AppTheme.sub.opacity(0.72))
                    .lineLimit(1).minimumScaleFactor(0.75)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 3)
            // 选中态：无描边软胶囊 + 阴影浮起（一体式导航条，不做单独玻璃圆框）
            .background {
                if selected {
                    Capsule()
                        .fill(AppTheme.selCapsule)
                        .shadow(color: .black.opacity(0.15), radius: 9, y: 4)
                }
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }

    // MARK: 右侧竖栏（折叠屏外屏：液态玻璃竖条，盖住系统时间/状态条区域）
    private func rightRail(systemEdge: CGFloat) -> some View {
        VStack(spacing: 4) {
            Spacer(minLength: systemEdge > 0 ? 170 : 12)   // 顶部让开系统的时间/连接圆环
            ForEach(0..<5, id: \.self) { i in railButton(i) }
            Spacer(minLength: 12)
        }
        .padding(.vertical, 8)
        .frame(width: max(66, systemEdge))
        .liquidGlass(Capsule(), light: true)   // zelda40.7：竖栏同底部，白天强制毛玻璃透明
        .padding(.vertical, 6).padding(.trailing, 4)
        .environment(\.colorScheme, .light)
    }

    private func railButton(_ i: Int) -> some View {
        let selected = page == i
        return Button {
            hideKeyboard()
            page = i
        } label: {
            VStack(spacing: 3) {
                Image(systemName: selected ? TabSpec.fillIcons[i] : TabSpec.icons[i])
                    .font(.system(size: 20, weight: .semibold))
                    .foregroundColor(selected ? AppTheme.tabAccents[i] : AppTheme.sub.opacity(0.72))
                    .frame(width: 46, height: 30)
                Text(TabSpec.titles[i])
                    .font(.system(size: 10, weight: selected ? .bold : .medium))
                    .foregroundColor(selected ? AppTheme.tabAccents[i] : AppTheme.sub.opacity(0.72))
                    .lineLimit(1).minimumScaleFactor(0.6)
            }
            .frame(width: 58)
            // 选中态：无描边软胶囊 + 阴影浮起，和底部横条一致
            .background {
                if selected {
                    Capsule()
                        .fill(AppTheme.selCapsule)
                        .shadow(color: .black.opacity(0.15), radius: 9, y: 4)
                }
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

// MARK: - 5 个 Tab 的标题和图标

enum TabSpec {
    static let titles = ["首页搜索", "圆屏显示", "导航语音", "圆屏设置", "地图 Key"]
    /// 未选中：线性图标（灰色）
    static let icons = ["magnifyingglass", "map", "speaker.wave.2", "gearshape", "key"]
    /// 选中：填充图标（彩色，参照一体式导航条的设计语言）
    static let fillIcons = ["magnifyingglass.circle.fill", "map.fill", "speaker.wave.2.fill", "gearshape.fill", "key.fill"]
}

// MARK: - 导航条位置：底部横条 / 右侧竖栏（iPhone Duo 折叠屏适配）

/// 用户偏好：0 自动，1 始终底部，2 始终右侧竖栏（「圆屏设置」里改）
enum TabPlacement: Int, CaseIterable {
    case auto = 0, bottom = 1, right = 2
    var name: String {
        switch self {
        case .auto: return "自动"
        case .bottom: return "底部"
        case .right: return "右侧"
        }
    }
}

/// 当前该把导航条放哪：外屏竖排右侧；展开横屏竖排右侧（苹果 Duo 规范：内屏横屏 bars 竖直）；
/// 展开竖屏 / 普通机型横排底部（内屏竖屏空间足够，bars 回归水平）
enum TabLayout {
    /// 宽屏（展开态、iPad）：内容限宽居中，别拉得太宽
    static func isWide(size: CGSize, hSizeClass: UserInterfaceSizeClass?) -> Bool {
        if let h = hSizeClass, h == .regular { return true }
        return min(size.width, size.height) >= 600
    }

    /// 当前 window 所在的物理屏幕（苹果 Duo 规范：避免直接引用 UIScreen.main，从 window scene 动态取）
    static var sceneScreen: UIScreen? {
        UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .first { $0.activationState == .foregroundActive }?.screen
            ?? UIScreen.main   // iOS 26 及以下兜底
    }

    /// 是否竖排在右侧：外屏（合盖两种形态）+ 内屏横屏（regular 宽 + 横向）
    static func useSideRail(size: CGSize, hSizeClass: UserInterfaceSizeClass?, placement: Int) -> Bool {
        switch TabPlacement(rawValue: placement) ?? .auto {
        case .bottom: return false
        case .right: return true
        case .auto: break
        }
        // 内屏横屏：苹果 Duo 规范——内屏横屏时导航条 / 工具条竖排贴侧边，竖屏才回水平
        if let h = hSizeClass, h == .regular { return size.width > size.height }
        let screen = sceneScreen
        let panelWidth = (screen?.nativeBounds.width ?? 0) / (screen?.nativeScale ?? 1)
        guard size.width > 0, panelWidth > 0 else { return false }
        // 外屏形态一：窗口明显小于物理面板 —— Duo 合盖时系统在外屏右侧留一条
        // 状态栏（时间 + 连接状态），App 窗口只有面板左边一块（466 面板 / 381 窗口 ≈ 1.22）
        if panelWidth / size.width > 1.15 { return true }
        // 外屏形态二：窗口铺满整块外屏 —— 外屏是“矮胖”竖屏（约 466×678），
        // 普通直板 iPhone 没有这种尺寸（17 是 393×852，Pro Max 是 440×956，都更瘦长）
        if size.width >= 430 && size.height <= 700 { return true }
        return false
    }
}

// MARK: - 小组件

struct CardView<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        VStack(alignment: .leading, spacing: 10) { content }
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 18).fill(ContentView.card))
            .overlay(RoundedRectangle(cornerRadius: 18).stroke(ContentView.stroke, lineWidth: 1))
    }
}

/// 一条路线的对比卡片：标签、时间、距离 · 红绿灯 · 收费
struct RouteCard: View {
    let option: NaviBridge.RouteOption
    let selected: Bool
    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(option.label).font(.caption).foregroundColor(selected ? ContentView.pink : ContentView.sub).lineLimit(1)
            Text(NaviBridge.durText(option.timeS)).font(.headline).foregroundColor(AppTheme.text).lineLimit(1).minimumScaleFactor(0.7)
            Text(NaviBridge.distText(option.length) + (option.lights > 0 ? " · \(option.lights)灯" : "") + (option.toll > 0 ? " · ¥\(option.toll)" : ""))
                .font(.caption2).foregroundColor(ContentView.sub).lineLimit(1).minimumScaleFactor(0.7)
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 12).fill(ContentView.bg))
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(selected ? ContentView.pink : ContentView.stroke, lineWidth: selected ? 2 : 1))
        .contentShape(Rectangle())
    }
}

/// 收起键盘
func hideKeyboard() {
    UIApplication.shared.sendAction(#selector(UIResponder.resignFirstResponder), to: nil, from: nil, for: nil)
}

// MARK: 塞尔达40周年特别版 启动画面（旗帜淡入 → 停留 → 淡出，共约 2.9s）
private struct ZeldaSplash: View {
    @State private var opacity: CGFloat = 0
    @State private var done = false
    var body: some View {
        Group {
            if !done {
                Image("Zelda40Background")
                    .resizable()
                    .scaledToFill()
                    .ignoresSafeArea()
                    .opacity(opacity)
                    .overlay(
                        Text("塞尔达40周年特别版")
                            .font(.title3.bold())
                            .foregroundColor(Color(red: 1.0, green: 0.78, blue: 0.24))
                            .opacity(opacity)
                            .padding(.bottom, 110),
                        alignment: .bottom
                    )
                    .onAppear {
                        withAnimation(.easeInOut(duration: 1.2)) { opacity = 1 }
                        DispatchQueue.main.asyncAfter(deadline: .now() + 2.2) {
                            withAnimation(.easeInOut(duration: 0.6)) { opacity = 0 }
                        }
                        DispatchQueue.main.asyncAfter(deadline: .now() + 2.9) { done = true }
                    }
            }
        }
    }
}
