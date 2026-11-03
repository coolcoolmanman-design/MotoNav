import SwiftUI
import MapKit
import UIKit

// MARK: - 行程 Tab 总览（方案 A 自适应日+夜）

struct TripsOverviewView: View {
    @ObservedObject var rides = RideRecorder.shared
    @State private var period: TripPeriod = .week
    @State private var selectedBar: Int? = nil
    @State private var dayDrill: Date? = nil

    private var stats: TripStatsSnapshot {
        TripStatsEngine.snapshot(period: period, from: rides.rides)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            header
            periodPicker
            heroCard
            metricGrid
            roadClassCard
            barCard
            moduleLinks
            recentSection
        }
        .background(
            NavigationLink(
                destination: Group {
                    if let dayDrill {
                        TripsDayDetailView(day: dayDrill)
                    } else {
                        EmptyView()
                    }
                },
                isActive: Binding(
                    get: { dayDrill != nil },
                    set: { if !$0 { dayDrill = nil } }
                )
            ) { EmptyView() }
            .hidden()
        )
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("行程")
                .font(.system(size: 28, weight: .heavy, design: .rounded))
                .foregroundColor(AppTheme.text)
            Text(period.overviewSubtitle)
                .font(.caption)
                .foregroundColor(AppTheme.sub)
        }
        .padding(.top, 8)
    }

    private var periodPicker: some View {
        HStack(spacing: 0) {
            ForEach(TripPeriod.allCases) { p in
                Button {
                    withAnimation(.easeInOut(duration: 0.18)) {
                        period = p
                        selectedBar = nil
                    }
                } label: {
                    Text(p.rawValue)
                        .font(.system(size: 13, weight: period == p ? .semibold : .medium))
                        .foregroundColor(AppTheme.text)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 8)
                        .background(
                            Group {
                                if period == p {
                                    Capsule().fill(AppTheme.card)
                                        .shadow(color: Color.black.opacity(0.08), radius: 4, y: 1)
                                }
                            }
                        )
                }
                .pressableFeedback(scale: 0.97, haptic: .light)
            }
        }
        .padding(3)
        .background(Capsule().fill(AppTheme.sunken))
    }

    private var heroCard: some View {
        ZStack(alignment: .leading) {
            RoundedRectangle(cornerRadius: 22, style: .continuous)
                .fill(
                    LinearGradient(
                        colors: [
                            Color(red: 0.06, green: 0.10, blue: 0.18),
                            Color(red: 0.02, green: 0.03, blue: 0.06)
                        ],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                )
            HStack(alignment: .bottom) {
                VStack(alignment: .leading, spacing: 8) {
                    Text(period.heroLabel)
                        .font(.caption.weight(.medium))
                        .foregroundColor(Color.white.opacity(0.72))
                    HStack(alignment: .firstTextBaseline, spacing: 4) {
                        Text(TripFormat.km(stats.meters))
                            .font(.system(size: 40, weight: .bold, design: .rounded))
                            .foregroundColor(.white)
                            .monospacedDigit()
                        Text("km")
                            .font(.title3.weight(.semibold))
                            .foregroundColor(AppTheme.chartCyan)
                    }
                    Text(heroSubline)
                        .font(.caption)
                        .foregroundColor(Color.white.opacity(0.78))
                        .lineLimit(2)
                }
                Spacer(minLength: 8)
                MiniSparkline(values: stats.barBuckets.map { $0.meters })
                    .frame(width: 110, height: 64)
                    .padding(.bottom, 6)
            }
            .padding(18)
        }
        .frame(minHeight: 132)
    }

    private var heroSubline: String {
        var parts: [String] = []
        if let d = stats.periodDeltaPercent {
            parts.append("较上期 \(TripFormat.percent(d))")
        }
        parts.append(String(format: "日均 %.1f km", stats.avgKmPerDay))
        parts.append("\(stats.count) 次导航")
        return parts.joined(separator: " · ")
    }

    private var metricGrid: some View {
        let cols = [GridItem(.flexible(), spacing: 10), GridItem(.flexible(), spacing: 10)]
        return LazyVGrid(columns: cols, spacing: 10) {
            metricTile("驾驶时间", TripFormat.duration(stats.duration),
                       accent: "↑ 均次 \(TripFormat.duration(stats.avgDurationPerTrip))", tint: AppTheme.green)
            metricTile("平均速度", TripFormat.speed(stats.avgSpeedKmh),
                       accent: String(format: "最高 %.0f km/h", stats.maxSpeedKmh), tint: AppTheme.green)
            metricTile("行程次数", "\(stats.count) 次",
                       accent: "连续驾驶 \(stats.streakDays) 天", tint: AppTheme.green)
            metricTile(
                "单次最长",
                stats.longest.map { TripFormat.kmUnit($0.meters) } ?? "—",
                accent: longestSubtitle,
                tint: AppTheme.sub
            )
        }
    }

    @ViewBuilder
    private var roadClassCard: some View {
        let split = stats.roadClass
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("城快 vs 高速").font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                Spacer()
                Text(split.identifiable ? "速度启发式" : "暂无法识别")
                    .font(.caption)
                    .foregroundColor(AppTheme.sub)
            }
            if split.identifiable {
                GeometryReader { geo in
                    HStack(spacing: 0) {
                        Rectangle()
                            .fill(AppTheme.accent)
                            .frame(width: max(4, geo.size.width * split.expressShare))
                        Rectangle()
                            .fill(AppTheme.chartCyan)
                            .frame(width: max(4, geo.size.width * split.highwayShare))
                    }
                    .clipShape(Capsule())
                }
                .frame(height: 10)
                HStack {
                    Text(String(format: "城快 %.0f%% · %.1f km", split.expressShare * 100, split.expressMeters / 1000))
                        .font(.caption).foregroundColor(AppTheme.sub)
                    Spacer()
                    Text(String(format: "高速 %.0f%% · %.1f km", split.highwayShare * 100, split.highwayMeters / 1000))
                        .font(.caption).foregroundColor(AppTheme.sub)
                }
            } else {
                Text("数据不足：当前区间缺少可持续的速度样本，无法诚实区分城快与高速。")
                    .font(.caption)
                    .foregroundColor(AppTheme.sub)
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 18, style: .continuous).fill(AppTheme.card))
    }

    private var longestSubtitle: String {
        guard let ride = stats.longest else { return "暂无记录" }
        let name = ride.mode.isEmpty ? "导航" : ride.mode
        return "\(name) · \(TripFormat.relativeDay(ride.started))"
    }

    private func metricTile(_ title: String, _ value: String, accent: String, tint: Color) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title).font(.caption).foregroundColor(AppTheme.sub)
            Text(value)
                .font(.system(size: 22, weight: .bold, design: .rounded))
                .foregroundColor(AppTheme.text)
                .monospacedDigit()
                .lineLimit(1)
                .minimumScaleFactor(0.7)
            Text(accent)
                .font(.caption2.weight(.medium))
                .foregroundColor(tint)
                .lineLimit(1)
                .minimumScaleFactor(0.8)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16, style: .continuous).fill(AppTheme.card))
    }

    private var barCard: some View {
        let buckets = stats.barBuckets
        let maxM = max(buckets.map(\.meters).max() ?? 0, 1)
        let sel = selectedBar.flatMap { buckets.indices.contains($0) ? buckets[$0] : nil }
        return VStack(alignment: .leading, spacing: 10) {
            Text(period.heroLabel)
                .font(.caption)
                .foregroundColor(AppTheme.sub)
            HStack(alignment: .bottom, spacing: 4) {
                ForEach(Array(buckets.enumerated()), id: \.element.id) { idx, b in
                    let h = CGFloat(b.meters / maxM)
                    Button {
                        withAnimation(.easeInOut(duration: 0.15)) { selectedBar = idx }
                        if let day = resolvedDay(for: b, index: idx) {
                            dayDrill = Calendar.current.startOfDay(for: day)
                        }
                    } label: {
                        VStack(spacing: 6) {
                            RoundedRectangle(cornerRadius: 5, style: .continuous)
                                .fill(
                                    LinearGradient(
                                        colors: selectedBar == idx
                                            ? [AppTheme.accent, AppTheme.chartCyan]
                                            : [AppTheme.chartCyan.opacity(0.95), AppTheme.accent.opacity(0.55)],
                                        startPoint: .top,
                                        endPoint: .bottom
                                    )
                                )
                                .frame(maxWidth: .infinity)
                                .frame(height: max(6, 96 * h))
                                .opacity(b.meters <= 0 ? 0.22 : 1)
                            Text(barAxisLabel(b.label, index: idx, total: buckets.count))
                                .font(.system(size: 10, weight: .medium))
                                .foregroundColor(selectedBar == idx ? AppTheme.accent : AppTheme.sub)
                        }
                    }
                    .pressableFeedback(scale: 0.96, haptic: .light)
                }
            }
            .frame(height: 122, alignment: .bottom)
            HStack {
                Text("点选柱进入单日明细")
                    .font(.caption2)
                    .foregroundColor(AppTheme.sub)
                Spacer()
                if let sel {
                    Text("\(sel.label)  \(TripFormat.kmUnit(sel.meters)) ›")
                        .font(.caption.weight(.semibold))
                        .foregroundColor(AppTheme.accent)
                }
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 18, style: .continuous).fill(AppTheme.card))
    }

    private func resolvedDay(for bucket: TripBucket, index: Int) -> Date? {
        if let d = bucket.date { return d }
        let cal = Calendar.current
        let now = Date()
        switch period {
        case .day:
            var comps = cal.dateComponents([.year, .month, .day], from: now)
            comps.hour = index
            return cal.date(from: comps)
        case .month:
            var comps = cal.dateComponents([.year, .month], from: now)
            comps.day = index + 1
            return cal.date(from: comps)
        case .week, .year, .all:
            return nil
        }
    }

    private func barAxisLabel(_ label: String, index: Int, total: Int) -> String {
        if total > 16 {
            // 月视图只标部分日
            if index == 0 || index == total - 1 || index % 4 == 0 { return label }
            return ""
        }
        return label
    }

    private var moduleLinks: some View {
        VStack(spacing: 10) {
            NavigationLink(destination: TripsChartsView(initialPeriod: period)) {
                moduleRow("图表", "里程 · 时长 · 时段 · 热力", "chart.bar.xaxis")
            }
            .pressableFeedback(scale: 0.97, haptic: .light)
            NavigationLink(destination: TripsFootprintView()) {
                moduleRow("足迹", "轨迹叠线 · TOP · 按日", "map")
            }
            .pressableFeedback(scale: 0.97, haptic: .light)
            NavigationLink(destination: TripsAnnualFootprintView()) {
                moduleRow("年度足迹", "全年轨迹专题图", "globe")
            }
            .pressableFeedback(scale: 0.97, haptic: .light)
            NavigationLink(destination: TripsInsightsView(initialPeriod: period)) {
                moduleRow("智能分析", "环比 · 习惯 · 城快高速", "sparkles")
            }
            .pressableFeedback(scale: 0.97, haptic: .light)
            NavigationLink(destination: TripsAnnualReportView()) {
                moduleRow("年度报告", "高清海报 · 可分享", "doc.richtext")
            }
            .pressableFeedback(scale: 0.97, haptic: .light)
        }
    }

    private func moduleRow(_ title: String, _ sub: String, _ icon: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: icon)
                .font(.system(size: 18, weight: .semibold))
                .foregroundColor(AppTheme.accent)
                .frame(width: 36, height: 36)
                .background(RoundedRectangle(cornerRadius: 10).fill(AppTheme.accentSoft))
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.body.weight(.semibold)).foregroundColor(AppTheme.text)
                Text(sub).font(.caption).foregroundColor(AppTheme.sub)
            }
            Spacer()
            Image(systemName: "chevron.right")
                .font(.caption.weight(.semibold))
                .foregroundColor(AppTheme.sub.opacity(0.7))
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16, style: .continuous).fill(AppTheme.card))
    }

    @ViewBuilder
    private var recentSection: some View {
        if let active = rides.active {
            CardView {
                Text("记录中 · \(active.mode) · \(Int(active.meters)) 米")
                    .foregroundColor(AppTheme.accent).bold()
                Text("真实导航或巡航中自动写入；模拟导航不记录")
                    .font(.caption).foregroundColor(AppTheme.sub)
            }
        }
        let real = TripStatsEngine.allRealRides(rides.rides)
        if let latest = real.first {
            Text("最近记录")
                .font(.subheadline.weight(.semibold))
                .foregroundColor(AppTheme.sub)
                .padding(.top, 4)
            NavigationLink(destination: RideDetailView(ride: latest)) {
                RideFeatureCard(ride: latest)
            }
            .pressableFeedback(scale: 0.97, haptic: .light)
        } else if rides.active == nil {
            CardView {
                Text("还没有导航记录")
                    .foregroundColor(AppTheme.text).bold()
                Text("真实导航或巡航开始时自动记录；模拟导航不记录。")
                    .font(.caption).foregroundColor(AppTheme.sub)
            }
        }
        if real.count > 1 {
            NavigationLink(destination: RideHistoryView()) {
                moduleRow("全部记录", "\(real.count) 次 · 导出 GPX", "list.bullet")
            }
            .pressableFeedback(scale: 0.97, haptic: .light)
        }
    }
}

// MARK: - 图表

struct TripsChartsView: View {
    @ObservedObject var rides = RideRecorder.shared
    @State var period: TripPeriod
    init(initialPeriod: TripPeriod) { _period = State(initialValue: initialPeriod) }

    private var stats: TripStatsSnapshot {
        TripStatsEngine.snapshot(period: period, from: rides.rides)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                periodChips
                chartCard(title: "里程", subtitle: "点柱看详情") {
                    SimpleBarChart(buckets: stats.barBuckets)
                }
                chartCard(title: "驾驶时间趋势", subtitle: period.rawValue) {
                    SimpleAreaChart(values: stats.barBuckets.map { $0.seconds / 60.0 })
                    HStack {
                        Text("累计 \(TripFormat.duration(stats.duration))")
                            .font(.caption).foregroundColor(AppTheme.sub)
                        Spacer()
                        let peak = stats.barBuckets.map(\.seconds).max() ?? 0
                        Text("峰值 \(TripFormat.duration(peak))")
                            .font(.caption.weight(.semibold)).foregroundColor(AppTheme.accent)
                    }
                }
                chartCard(title: "出行时段", subtitle: "工作日 vs 周末") {
                    HourHistogram(bins: stats.hourHistogram)
                    Text(peakHourCaption)
                        .font(.caption).foregroundColor(AppTheme.sub)
                }
                chartCard(title: "日历热力", subtitle: "近 16 周 · 类贡献图") {
                    HeatmapGrid(grid: stats.heatmap)
                }
            }
            .padding(16)
            .padding(.bottom, 40)
        }
        .background(AppTheme.bg.ignoresSafeArea())
        .navigationTitle("图表")
        .navigationBarTitleDisplayMode(.inline)
        .background(InteractivePopEnabler())
    }

    private var periodChips: some View {
        HStack(spacing: 0) {
            ForEach(TripPeriod.allCases) { p in
                Button {
                    period = p
                } label: {
                    Text(p.rawValue)
                        .font(.system(size: 13, weight: period == p ? .semibold : .medium))
                        .foregroundColor(AppTheme.text)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 8)
                        .background {
                            if period == p {
                                Capsule().fill(AppTheme.card)
                            }
                        }
                }
                .pressableFeedback(scale: 0.97, haptic: .light)
            }
        }
        .padding(3)
        .background(Capsule().fill(AppTheme.sunken))
    }

    private var peakHourCaption: String {
        guard let slot = TripStatsEngine.peakHourRange(rides: stats.rides) else {
            return "暂无高峰数据"
        }
        return String(format: "高峰 %02d-%02d", slot.start, slot.end)
    }

    private func chartCard<Content: View>(title: String, subtitle: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text(title).font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                Spacer()
                Text(subtitle).font(.caption).foregroundColor(AppTheme.sub)
            }
            content()
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 18, style: .continuous).fill(AppTheme.card))
    }
}

// MARK: - 足迹

struct TripsFootprintView: View {
    @ObservedObject var rides = RideRecorder.shared
    @State private var filter: FootprintFilter = .all
    @State private var dayDrill: Date? = nil

    enum FootprintFilter: String, CaseIterable, Identifiable {
        case all = "全部轨迹"
        case top = "TOP 10"
        case longest = "最长"
        case year = "年度"
        var id: String { rawValue }
    }

    private var real: [RideRecorder.Ride] { TripStatsEngine.allRealRides(rides.rides) }

    private var shown: [RideRecorder.Ride] {
        switch filter {
        case .all: return Array(real.prefix(40))
        case .top:
            // 取次数最高的 mode 下的近期样本
            let keys = Set(TripStatsEngine.topRoutes(rides: real, limit: 10).map(\.title))
            return Array(real.filter { ride in
                let key: String
                if ride.mode.isEmpty {
                    let bucket = Int((ride.meters / 1000.0).rounded())
                    key = bucket <= 0 ? "短途导航" : "约 \(bucket) km 路线"
                } else {
                    key = ride.mode
                }
                return keys.contains(key)
            }.prefix(20))
        case .longest:
            return Array(real.sorted { $0.meters > $1.meters }.prefix(12))
        case .year:
            let y = Calendar.current.component(.year, from: Date())
            return real.filter { Calendar.current.component(.year, from: $0.started) == y }
        }
    }

    private var mapFixes: [RideRecorder.Fix] {
        Array(shown.prefix(8).flatMap(\.fixes).prefix(4000))
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                filterRow
                NavigationLink(destination: TripsAnnualFootprintView()) {
                    moduleBanner("年度足迹专题", "全年轨迹叠线 · 按月摘要", "globe")
                }
                .pressableFeedback(scale: 0.97, haptic: .light)

                ZStack(alignment: .topLeading) {
                    RideTrackMapView(fixes: mapFixes, interactive: true, lineWidth: 3.5, compactMarkers: true)
                        .frame(height: 280)
                        .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))
                    if let latest = shown.first {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(rideTitle(latest)).font(.caption.weight(.semibold)).foregroundColor(AppTheme.text)
                            Text("\(TripFormat.relativeDay(latest.started)) · \(TripFormat.kmUnit(latest.meters)) · \(durationText(latest))")
                                .font(.caption2).foregroundColor(AppTheme.sub)
                        }
                        .padding(10)
                        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                        .padding(12)
                    } else {
                        Text("暂无真实导航轨迹")
                            .font(.caption)
                            .foregroundColor(AppTheme.sub)
                            .padding(12)
                    }
                }
                .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))

                // 按日入口：最近有记录的日子
                VStack(alignment: .leading, spacing: 10) {
                    Text("按日下钻")
                        .font(.caption)
                        .foregroundColor(AppTheme.sub)
                    let days = recentDays(limit: 8)
                    if days.isEmpty {
                        Text("暂无按日记录").font(.subheadline).foregroundColor(AppTheme.sub)
                    } else {
                        ForEach(days, id: \.self) { day in
                            Button {
                                dayDrill = day
                            } label: {
                                let subset = TripStatsEngine.rides(on: day, from: rides.rides)
                                HStack {
                                    Text(TripFormat.relativeDay(day))
                                        .font(.subheadline.weight(.semibold))
                                        .foregroundColor(AppTheme.text)
                                    Spacer()
                                    Text("\(subset.count) 次 · \(TripFormat.kmUnit(subset.reduce(0) { $0 + $1.meters }))")
                                        .font(.caption)
                                        .foregroundColor(AppTheme.sub)
                                    Image(systemName: "chevron.right")
                                        .font(.caption.weight(.semibold))
                                        .foregroundColor(AppTheme.sub.opacity(0.7))
                                }
                                .padding(.vertical, 6)
                            }
                            .pressableFeedback(scale: 0.98, haptic: .light)
                        }
                    }
                }
                .padding(14)
                .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))

                VStack(alignment: .leading, spacing: 10) {
                    Text("常用起终点 · TOP")
                        .font(.caption)
                        .foregroundColor(AppTheme.sub)
                    let tops = TripStatsEngine.topRoutes(rides: real, limit: 10)
                    if tops.isEmpty {
                        Text("数据不足")
                            .font(.subheadline)
                            .foregroundColor(AppTheme.sub)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(.vertical, 8)
                    } else {
                        ForEach(Array(tops.enumerated()), id: \.offset) { idx, item in
                            HStack(spacing: 10) {
                                Text("\(idx + 1)")
                                    .font(.caption.weight(.bold))
                                    .foregroundColor(AppTheme.accent)
                                    .frame(width: 28, height: 28)
                                    .background(RoundedRectangle(cornerRadius: 8).fill(AppTheme.accentSoft))
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(item.title).font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                                    Text(String(format: "均 %.1f km", item.avgKm)).font(.caption).foregroundColor(AppTheme.sub)
                                }
                                Spacer()
                                VStack(alignment: .trailing, spacing: 2) {
                                    Text("\(item.count) 次").font(.subheadline.weight(.bold)).foregroundColor(AppTheme.text)
                                    Text("样本").font(.caption2).foregroundColor(AppTheme.sub)
                                }
                            }
                            if idx < tops.count - 1 { Divider().opacity(0.3) }
                        }
                    }
                }
                .padding(14)
                .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))

                ForEach(shown.prefix(12)) { ride in
                    NavigationLink(destination: RideDetailView(ride: ride)) {
                        RideListRow(ride: ride)
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                            .background(RoundedRectangle(cornerRadius: 14).fill(AppTheme.card))
                    }
                    .pressableFeedback(scale: 0.97, haptic: .light)
                }
            }
            .padding(16)
            .padding(.bottom, 40)
        }
        .background(AppTheme.bg.ignoresSafeArea())
        .navigationTitle("足迹")
        .navigationBarTitleDisplayMode(.inline)
        .background(InteractivePopEnabler())
        .background(
            NavigationLink(
                destination: Group {
                    if let dayDrill { TripsDayDetailView(day: dayDrill) } else { EmptyView() }
                },
                isActive: Binding(get: { dayDrill != nil }, set: { if !$0 { dayDrill = nil } })
            ) { EmptyView() }.hidden()
        )
    }

    private func recentDays(limit: Int) -> [Date] {
        let cal = Calendar.current
        var seen = Set<Date>()
        var out: [Date] = []
        for ride in real {
            let d = cal.startOfDay(for: ride.started)
            if seen.insert(d).inserted {
                out.append(d)
                if out.count >= limit { break }
            }
        }
        return out
    }

    private func moduleBanner(_ title: String, _ sub: String, _ icon: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: icon)
                .font(.system(size: 18, weight: .semibold))
                .foregroundColor(AppTheme.accent)
                .frame(width: 36, height: 36)
                .background(RoundedRectangle(cornerRadius: 10).fill(AppTheme.accentSoft))
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.body.weight(.semibold)).foregroundColor(AppTheme.text)
                Text(sub).font(.caption).foregroundColor(AppTheme.sub)
            }
            Spacer()
            Image(systemName: "chevron.right")
                .font(.caption.weight(.semibold))
                .foregroundColor(AppTheme.sub.opacity(0.7))
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16, style: .continuous).fill(AppTheme.card))
    }

    private var filterRow: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                ForEach(FootprintFilter.allCases) { f in
                    Button {
                        filter = f
                    } label: {
                        Text(f.rawValue)
                            .font(.caption.weight(filter == f ? .semibold : .medium))
                            .foregroundColor(filter == f ? .white : AppTheme.text)
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                            .background(
                                Capsule().fill(filter == f ? AppTheme.accent : AppTheme.secondaryFill)
                            )
                    }
                    .pressableFeedback(scale: 0.95, haptic: .light)
                }
            }
        }
    }
}

// MARK: - 智能分析

struct TripsInsightsView: View {
    @ObservedObject var rides = RideRecorder.shared
    @State var period: TripPeriod
    init(initialPeriod: TripPeriod) { _period = State(initialValue: initialPeriod == .day ? .month : initialPeriod) }

    private var stats: TripStatsSnapshot {
        TripStatsEngine.snapshot(period: period, from: rides.rides)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                if !stats.dataSufficientForInsights {
                    insufficientCard
                } else {
                    HStack(spacing: 10) {
                        deltaCard(
                            title: "环比里程",
                            value: stats.periodDeltaPercent.map(TripFormat.percent) ?? "—",
                            detail: String(format: "%.0f → %.0f km", stats.previousMeters / 1000, stats.km),
                            positive: (stats.periodDeltaPercent ?? 0) >= 0
                        )
                        deltaCard(
                            title: "环比次数",
                            value: stats.periodCountDelta >= 0 ? "+\(stats.periodCountDelta)" : "\(stats.periodCountDelta)",
                            detail: "较上期",
                            positive: stats.periodCountDelta >= 0
                        )
                    }
                    VStack(alignment: .leading, spacing: 12) {
                        ForEach(stats.insights) { item in
                            HStack(alignment: .top, spacing: 12) {
                                Text(item.icon)
                                    .font(.caption.weight(.bold))
                                    .foregroundColor(AppTheme.accent)
                                    .frame(width: 32, height: 32)
                                    .background(RoundedRectangle(cornerRadius: 8).fill(AppTheme.accentSoft))
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(item.title).font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                                    Text(item.detail).font(.caption).foregroundColor(AppTheme.sub)
                                }
                            }
                        }
                    }
                    .padding(14)
                    .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))

                    let share = stats.weekdayShare
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Text("工作日 vs 周末").font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                            Spacer()
                            Text("里程占比").font(.caption).foregroundColor(AppTheme.sub)
                        }
                        GeometryReader { geo in
                            HStack(spacing: 0) {
                                Rectangle()
                                    .fill(LinearGradient(colors: [AppTheme.accent, AppTheme.chartCyan], startPoint: .leading, endPoint: .trailing))
                                    .frame(width: max(4, geo.size.width * share.work))
                                Rectangle()
                                    .fill(AppTheme.secondaryFill)
                                    .frame(width: max(4, geo.size.width * share.weekend))
                            }
                            .clipShape(Capsule())
                        }
                        .frame(height: 12)
                        HStack {
                            Text(String(format: "工作日 %.0f%%", share.work * 100)).font(.caption).foregroundColor(AppTheme.sub)
                            Spacer()
                            Text(String(format: "周末 %.0f%%", share.weekend * 100)).font(.caption).foregroundColor(AppTheme.sub)
                        }
                    }
                    .padding(14)
                    .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))

                    VStack(alignment: .leading, spacing: 6) {
                        Text("习惯摘要").font(.caption).foregroundColor(AppTheme.sub)
                        Text(stats.habitSummary)
                            .font(.subheadline)
                            .foregroundColor(AppTheme.text)
                    }
                    .padding(14)
                    .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))

                    roadClassInsightCard
                }
            }
            .padding(16)
            .padding(.bottom, 40)
        }
        .background(AppTheme.bg.ignoresSafeArea())
        .navigationTitle("智能分析")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .navigationBarTrailing) {
                Menu {
                    ForEach(TripPeriod.allCases) { p in
                        Button(p.rawValue) { period = p }
                    }
                } label: {
                    Text(period.rawValue).foregroundColor(AppTheme.accent)
                }
            }
        }
        .background(InteractivePopEnabler())
    }

    private var roadClassInsightCard: some View {
        let split = stats.roadClass
        return VStack(alignment: .leading, spacing: 10) {
            Text("城快 vs 高速").font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
            if split.identifiable {
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("城快").font(.caption).foregroundColor(AppTheme.sub)
                        Text(String(format: "%.0f%%", split.expressShare * 100))
                            .font(.title3.weight(.bold)).foregroundColor(AppTheme.accent)
                        Text(TripFormat.kmUnit(split.expressMeters)).font(.caption2).foregroundColor(AppTheme.sub)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    VStack(alignment: .leading, spacing: 4) {
                        Text("高速").font(.caption).foregroundColor(AppTheme.sub)
                        Text(String(format: "%.0f%%", split.highwayShare * 100))
                            .font(.title3.weight(.bold)).foregroundColor(AppTheme.chartCyan)
                        Text(TripFormat.kmUnit(split.highwayMeters)).font(.caption2).foregroundColor(AppTheme.sub)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                Text("依据持续 GPS 速度启发式（≥80 高速 / 50–80 城快），非官方路网等级。")
                    .font(.caption2).foregroundColor(AppTheme.sub)
            } else {
                Text("数据不足")
                    .font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                Text("当前区间缺少可持续速度样本，暂无法识别城快与高速。")
                    .font(.caption).foregroundColor(AppTheme.sub)
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))
    }

    private var insufficientCard: some View {
        VStack(spacing: 12) {
            Image(systemName: "chart.bar.doc.horizontal")
                .font(.system(size: 36, weight: .light))
                .foregroundColor(AppTheme.sub)
            Text("数据不足")
                .font(.title3.weight(.bold))
                .foregroundColor(AppTheme.text)
            Text("智能分析至少需要 3 次真实导航记录。当前 \(stats.count) 次；模拟导航不计入。")
                .font(.subheadline)
                .foregroundColor(AppTheme.sub)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .padding(28)
        .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))
    }

    private func deltaCard(title: String, value: String, detail: String, positive: Bool) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title).font(.caption).foregroundColor(AppTheme.sub)
            Text(value)
                .font(.system(size: 28, weight: .bold, design: .rounded))
                .foregroundColor(positive ? AppTheme.green : AppTheme.text)
            Text(detail).font(.caption).foregroundColor(positive ? AppTheme.green : AppTheme.sub)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(AppTheme.card))
    }
}

// MARK: - 单日明细

struct TripsDayDetailView: View {
    let day: Date
    @ObservedObject var rides = RideRecorder.shared

    private var dayRides: [RideRecorder.Ride] {
        TripStatsEngine.rides(on: day, from: rides.rides)
    }

    private var meters: Double { dayRides.reduce(0) { $0 + $1.meters } }
    private var duration: TimeInterval {
        dayRides.reduce(0) { $0 + TripStatsSnapshot.duration(of: $1) }
    }
    private var mapFixes: [RideRecorder.Fix] {
        Array(dayRides.flatMap(\.fixes).prefix(5000))
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                Text(dayTitle)
                    .font(.title2.weight(.bold))
                    .foregroundColor(AppTheme.text)
                Text("真实 GPS · 不含模拟导航")
                    .font(.caption)
                    .foregroundColor(AppTheme.sub)

                HStack(spacing: 10) {
                    dayMetric("里程", TripFormat.kmUnit(meters))
                    dayMetric("时长", TripFormat.duration(duration))
                    dayMetric("次数", "\(dayRides.count)")
                }

                RideTrackMapView(fixes: mapFixes, interactive: true, lineWidth: 4, compactMarkers: true)
                    .frame(height: 260)
                    .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))

                if dayRides.isEmpty {
                    Text("这一天没有真实导航记录。")
                        .font(.subheadline)
                        .foregroundColor(AppTheme.sub)
                        .padding(14)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(RoundedRectangle(cornerRadius: 16).fill(AppTheme.card))
                } else {
                    ForEach(dayRides) { ride in
                        NavigationLink(destination: RideDetailView(ride: ride)) {
                            RideListRow(ride: ride)
                                .padding(.horizontal, 12)
                                .padding(.vertical, 8)
                                .background(RoundedRectangle(cornerRadius: 14).fill(AppTheme.card))
                        }
                        .pressableFeedback(scale: 0.97, haptic: .light)
                    }
                }
            }
            .padding(16)
            .padding(.bottom, 40)
        }
        .background(AppTheme.bg.ignoresSafeArea())
        .navigationTitle("单日明细")
        .navigationBarTitleDisplayMode(.inline)
        .background(InteractivePopEnabler())
    }

    private var dayTitle: String {
        let f = DateFormatter(); f.dateFormat = "yyyy年M月d日"
        return f.string(from: day)
    }

    private func dayMetric(_ t: String, _ v: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(t).font(.caption).foregroundColor(AppTheme.sub)
            Text(v).font(.headline.weight(.bold)).foregroundColor(AppTheme.text)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 14).fill(AppTheme.card))
    }
}

// MARK: - 年度足迹专题

struct TripsAnnualFootprintView: View {
    @ObservedObject var rides = RideRecorder.shared
    @State private var year: Int = Calendar.current.component(.year, from: Date())

    private var yearRides: [RideRecorder.Ride] {
        let cal = Calendar.current
        return TripStatsEngine.allRealRides(rides.rides).filter { cal.component(.year, from: $0.started) == year }
    }

    private var mapFixes: [RideRecorder.Fix] {
        Array(yearRides.prefix(60).flatMap(\.fixes).prefix(8000))
    }

    private var monthly: [(Int, Double, Int)] {
        let cal = Calendar.current
        return (1...12).map { m in
            let subset = yearRides.filter { cal.component(.month, from: $0.started) == m }
            return (m, subset.reduce(0) { $0 + $1.meters }, subset.count)
        }
    }

    private var availableYears: [Int] {
        let cal = Calendar.current
        let ys = Set(TripStatsEngine.allRealRides(rides.rides).map { cal.component(.year, from: $0.started) })
        let cur = cal.component(.year, from: Date())
        return (ys.union([cur])).sorted(by: >)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                HStack {
                    Text("\(year) 年度足迹")
                        .font(.title3.weight(.bold))
                        .foregroundColor(AppTheme.text)
                    Spacer()
                    Menu {
                        ForEach(availableYears, id: \.self) { y in
                            Button("\(y)") { year = y }
                        }
                    } label: {
                        Text("切换年份").font(.caption.weight(.semibold)).foregroundColor(AppTheme.accent)
                    }
                }

                ZStack(alignment: .bottomLeading) {
                    RideTrackMapView(fixes: mapFixes, interactive: true, lineWidth: 2.8, compactMarkers: true)
                        .frame(height: 340)
                        .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))
                    VStack(alignment: .leading, spacing: 2) {
                        Text(String(format: "%.0f km · %d 次", yearRides.reduce(0) { $0 + $1.meters } / 1000, yearRides.count))
                            .font(.caption.weight(.semibold))
                            .foregroundColor(AppTheme.text)
                        Text("全年真实导航叠线").font(.caption2).foregroundColor(AppTheme.sub)
                    }
                    .padding(10)
                    .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
                    .padding(12)
                }

                VStack(alignment: .leading, spacing: 10) {
                    Text("按月摘要").font(.caption).foregroundColor(AppTheme.sub)
                    ForEach(monthly.filter { $0.2 > 0 }, id: \.0) { item in
                        HStack {
                            Text("\(item.0) 月").font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                            Spacer()
                            Text("\(item.2) 次 · \(TripFormat.kmUnit(item.1))")
                                .font(.caption).foregroundColor(AppTheme.sub)
                        }
                        Divider().opacity(0.25)
                    }
                    if yearRides.isEmpty {
                        Text("这一年还没有真实导航足迹。")
                            .font(.subheadline).foregroundColor(AppTheme.sub)
                    }
                }
                .padding(14)
                .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))
            }
            .padding(16)
            .padding(.bottom, 40)
        }
        .background(AppTheme.bg.ignoresSafeArea())
        .navigationTitle("年度足迹")
        .navigationBarTitleDisplayMode(.inline)
        .background(InteractivePopEnabler())
    }
}

// MARK: - 年度报告

struct TripsAnnualReportView: View {
    @ObservedObject var rides = RideRecorder.shared
    @State private var shareItems: [Any] = []
    @State private var showShare = false
    @State private var shareError = ""

    private var report: (
        year: Int, km: Double, hours: Double, count: Int, avgSpeed: Double, longestKm: Double, topMode: String, activeMonth: Int
    ) {
        TripStatsEngine.annual(rides: rides.rides)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                poster
                NavigationLink(destination: TripsAnnualFootprintView()) {
                    HStack {
                        Image(systemName: "globe")
                        Text("查看年度足迹专题图")
                        Spacer()
                        Image(systemName: "chevron.right")
                    }
                    .font(.subheadline.weight(.semibold))
                    .foregroundColor(AppTheme.accent)
                    .padding(14)
                    .background(RoundedRectangle(cornerRadius: 14).fill(AppTheme.card))
                }
                .pressableFeedback(scale: 0.97, haptic: .light)

                Button {
                    prepareShare()
                } label: {
                    Text("分享高清海报")
                        .font(.headline)
                        .foregroundColor(.white)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                        .background(RoundedRectangle(cornerRadius: 14).fill(AppTheme.accent))
                }
                .pressableFeedback(scale: 0.96, haptic: .medium)

                if !shareError.isEmpty {
                    Text(shareError).font(.caption).foregroundColor(AppTheme.red)
                }
                Text("分享优先提供 JPEG 海报文件（微信可接收）；不含模拟导航。")
                    .font(.caption2)
                    .foregroundColor(AppTheme.sub)
            }
            .padding(16)
            .padding(.bottom, 40)
        }
        .background(AppTheme.bg.ignoresSafeArea())
        .navigationTitle("年度报告")
        .navigationBarTitleDisplayMode(.inline)
        .background(InteractivePopEnabler())
        .sheet(isPresented: $showShare) {
            ActivityShareSheet(items: shareItems)
        }
    }

    private var poster: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("MOTONAV · 行程报告")
                .font(.caption)
                .foregroundColor(Color.white.opacity(0.55))
            Text("你的 \(report.year) 骑行一年")
                .font(.title.weight(.bold))
                .foregroundColor(.white)
            Text("基于真实 GPS 导航记录")
                .font(.caption.weight(.medium))
                .foregroundColor(AppTheme.chartCyan)
            VStack(alignment: .leading, spacing: 4) {
                Text("总里程").font(.caption).foregroundColor(Color.white.opacity(0.55))
                HStack(alignment: .firstTextBaseline, spacing: 4) {
                    Text(String(format: report.km >= 100 ? "%.0f" : "%.1f", report.km))
                        .font(.system(size: 44, weight: .bold, design: .rounded))
                        .foregroundColor(.white)
                    Text("km").font(.title3.weight(.semibold)).foregroundColor(AppTheme.chartCyan)
                }
            }
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible())], spacing: 14) {
                mini("驾驶时长", String(format: "%.0f h", report.hours))
                mini("行程次数", "\(report.count)")
                mini("平均速度", String(format: "%.1f", report.avgSpeed))
                mini("单次最长", String(format: "%.0f km", report.longestKm))
                mini("最常地点", report.topMode)
                mini("最活跃月", report.activeMonth > 0 ? "\(report.activeMonth) 月" : "—")
            }
            MiniSparkline(values: monthlyKm)
                .frame(height: 56)
                .padding(12)
                .background(RoundedRectangle(cornerRadius: 12).fill(Color.white.opacity(0.06)))
            HStack {
                Text("高清海报可分享").font(.caption2).foregroundColor(Color.white.opacity(0.45))
                Spacer()
                Text("不含模拟导航").font(.caption2).foregroundColor(Color.white.opacity(0.45))
            }
        }
        .padding(20)
        .background(
            LinearGradient(
                colors: [Color(red: 0.05, green: 0.09, blue: 0.16), Color(red: 0.02, green: 0.03, blue: 0.06)],
                startPoint: .top,
                endPoint: .bottom
            )
        )
        .clipShape(RoundedRectangle(cornerRadius: 22, style: .continuous))
    }

    private func mini(_ t: String, _ v: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(t).font(.caption2).foregroundColor(Color.white.opacity(0.5))
            Text(v).font(.subheadline.weight(.bold)).foregroundColor(.white).lineLimit(1).minimumScaleFactor(0.7)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var monthlyKm: [Double] {
        let cal = Calendar.current
        let y = report.year
        let subset = TripStatsEngine.allRealRides(rides.rides).filter { cal.component(.year, from: $0.started) == y }
        return (1...12).map { m in
            subset.filter { cal.component(.month, from: $0.started) == m }.reduce(0) { $0 + $1.meters }
        }
    }

    private func prepareShare() {
        shareError = ""
        let image = TripPosterRenderer.render(
            year: report.year,
            km: report.km,
            hours: report.hours,
            count: report.count,
            avgSpeed: report.avgSpeed,
            longestKm: report.longestKm,
            topMode: report.topMode,
            activeMonth: report.activeMonth,
            monthlyMeters: monthlyKm
        )
        do {
            let url = try TripPosterRenderer.writeJPEG(image, year: report.year)
            // 微信等扩展更稳接收「图片文件 URL」；同时提供 UIImage 兼容相册/邮件。
            shareItems = [ShareablePosterItem(image: image, fileURL: url, caption: shareCaption)]
            showShare = true
        } catch {
            shareError = "海报导出失败：\(error.localizedDescription)"
        }
    }

    private var shareCaption: String {
        "MotoNav \(report.year) 行程报告 · \(String(format: "%.1f", report.km)) km · \(report.count) 次（不含模拟）"
    }
}

// MARK: - 海报位图 + 分享（微信友好）

enum TripPosterRenderer {
    static func render(
        year: Int, km: Double, hours: Double, count: Int, avgSpeed: Double,
        longestKm: Double, topMode: String, activeMonth: Int, monthlyMeters: [Double]
    ) -> UIImage {
        let size = CGSize(width: 1080, height: 1920)
        let r = UIGraphicsImageRenderer(size: size)
        return r.image { ctx in
            let cg = ctx.cgContext
            let colors = [
                UIColor(red: 0.05, green: 0.09, blue: 0.16, alpha: 1).cgColor,
                UIColor(red: 0.02, green: 0.03, blue: 0.06, alpha: 1).cgColor
            ]
            if let g = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: colors as CFArray, locations: [0, 1]) {
                cg.drawLinearGradient(g, start: .zero, end: CGPoint(x: 0, y: size.height), options: [])
            }

            func draw(_ text: String, x: CGFloat, y: CGFloat, size fs: CGFloat, weight: UIFont.Weight, color: UIColor) {
                let attrs: [NSAttributedString.Key: Any] = [
                    .font: UIFont.systemFont(ofSize: fs, weight: weight),
                    .foregroundColor: color
                ]
                (text as NSString).draw(at: CGPoint(x: x, y: y), withAttributes: attrs)
            }

            draw("MOTONAV · 行程报告", x: 72, y: 120, size: 28, weight: .medium, color: UIColor.white.withAlphaComponent(0.55))
            draw("你的 \(year) 骑行一年", x: 72, y: 180, size: 56, weight: .bold, color: .white)
            draw("基于真实 GPS 导航记录", x: 72, y: 260, size: 28, weight: .semibold, color: UIColor(red: 0.39, green: 0.82, blue: 1, alpha: 1))

            draw("总里程", x: 72, y: 360, size: 26, weight: .medium, color: UIColor.white.withAlphaComponent(0.55))
            let kmText = String(format: km >= 100 ? "%.0f" : "%.1f", km)
            draw(kmText, x: 72, y: 400, size: 120, weight: .bold, color: .white)
            draw("km", x: 72 + CGFloat(kmText.count) * 62, y: 460, size: 40, weight: .semibold, color: UIColor(red: 0.39, green: 0.82, blue: 1, alpha: 1))

            let cells: [(String, String)] = [
                ("驾驶时长", String(format: "%.0f h", hours)),
                ("行程次数", "\(count)"),
                ("平均速度", String(format: "%.1f", avgSpeed)),
                ("单次最长", String(format: "%.0f km", longestKm)),
                ("最常地点", topMode),
                ("最活跃月", activeMonth > 0 ? "\(activeMonth) 月" : "—")
            ]
            for (i, cell) in cells.enumerated() {
                let col = i % 2
                let row = i / 2
                let x: CGFloat = 72 + CGFloat(col) * 480
                let y: CGFloat = 620 + CGFloat(row) * 140
                draw(cell.0, x: x, y: y, size: 24, weight: .medium, color: UIColor.white.withAlphaComponent(0.5))
                draw(cell.1, x: x, y: y + 40, size: 40, weight: .bold, color: .white)
            }

            // 月度火花线
            let maxM = max(monthlyMeters.max() ?? 1, 1)
            let origin = CGPoint(x: 72, y: 1280)
            let chartW: CGFloat = 936, chartH: CGFloat = 180
            cg.setStrokeColor(UIColor(red: 0.39, green: 0.82, blue: 1, alpha: 1).cgColor)
            cg.setLineWidth(6)
            cg.setLineJoin(.round)
            cg.setLineCap(.round)
            let vals = monthlyMeters.isEmpty ? [0.0] : monthlyMeters
            cg.beginPath()
            for (i, v) in vals.enumerated() {
                let x = origin.x + chartW * CGFloat(i) / CGFloat(max(vals.count - 1, 1))
                let y = origin.y + chartH - chartH * CGFloat(v / maxM)
                if i == 0 { cg.move(to: CGPoint(x: x, y: y)) } else { cg.addLine(to: CGPoint(x: x, y: y)) }
            }
            cg.strokePath()

            draw("不含模拟导航 · MotoNav", x: 72, y: 1750, size: 24, weight: .medium, color: UIColor.white.withAlphaComponent(0.4))
        }
    }

    static func writeJPEG(_ image: UIImage, year: Int) throws -> URL {
        guard let data = image.jpegData(compressionQuality: 0.92) else {
            throw NSError(domain: "TripPoster", code: 1, userInfo: [NSLocalizedDescriptionKey: "JPEG 编码失败"])
        }
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("MotoNav-\(year)-poster.jpg")
        try data.write(to: url, options: .atomic)
        return url
    }
}

/// 微信分享：优先交出 JPEG 文件 URL；其它活动可拿 UIImage。纯文字分享对微信易失败。
final class ShareablePosterItem: NSObject, UIActivityItemSource {
    let image: UIImage
    let fileURL: URL
    let caption: String

    init(image: UIImage, fileURL: URL, caption: String) {
        self.image = image
        self.fileURL = fileURL
        self.caption = caption
    }

    func activityViewControllerPlaceholderItem(_ activityViewController: UIActivityViewController) -> Any {
        image
    }

    func activityViewController(_ activityViewController: UIActivityViewController, itemForActivityType activityType: UIActivity.ActivityType?) -> Any? {
        let raw = activityType?.rawValue.lowercased() ?? ""
        if raw.contains("wechat") || raw.contains("weixin") || raw.contains("tencent") {
            return fileURL
        }
        if activityType == .mail || activityType == .message {
            return fileURL
        }
        return image
    }

    func activityViewController(_ activityViewController: UIActivityViewController, subjectForActivityType activityType: UIActivity.ActivityType?) -> String {
        caption
    }

    func activityViewController(_ activityViewController: UIActivityViewController, dataTypeIdentifierForActivityType activityType: UIActivity.ActivityType?) -> String {
        "public.jpeg"
    }

    func activityViewController(_ activityViewController: UIActivityViewController, thumbnailImageForActivityType activityType: UIActivity.ActivityType?, suggestedSize size: CGSize) -> UIImage? {
        image
    }
}

struct ActivityShareSheet: UIViewControllerRepresentable {
    let items: [Any]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        let vc = UIActivityViewController(activityItems: items, applicationActivities: nil)
        vc.excludedActivityTypes = [
            .addToReadingList,
            .assignToContact,
            .print
        ]
        if let pop = vc.popoverPresentationController {
            let scenes = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
            let root = scenes.first?.windows.first { $0.isKeyWindow }?.rootViewController?.view
            pop.sourceView = root
            pop.sourceRect = CGRect(x: (root?.bounds.midX ?? 0), y: (root?.bounds.midY ?? 0), width: 1, height: 1)
            pop.permittedArrowDirections = []
        }
        return vc
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
}

// MARK: - Chart primitives

struct MiniSparkline: View {
    let values: [Double]
    var body: some View {
        GeometryReader { geo in
            let vals = values.isEmpty ? [0.0] : values
            let maxV = max(vals.max() ?? 1, 1)
            let step = geo.size.width / CGFloat(max(vals.count - 1, 1))
            Path { p in
                for (i, v) in vals.enumerated() {
                    let x = CGFloat(i) * step
                    let y = geo.size.height - CGFloat(v / maxV) * geo.size.height
                    if i == 0 { p.move(to: CGPoint(x: x, y: y)) }
                    else { p.addLine(to: CGPoint(x: x, y: y)) }
                }
            }
            .stroke(AppTheme.chartCyan, style: StrokeStyle(lineWidth: 2.5, lineCap: .round, lineJoin: .round))
            .shadow(color: AppTheme.chartCyan.opacity(0.55), radius: 6)
        }
    }
}

struct SimpleBarChart: View {
    let buckets: [TripBucket]
    var body: some View {
        let maxM = max(buckets.map(\.meters).max() ?? 1, 1)
        HStack(alignment: .bottom, spacing: 3) {
            ForEach(buckets) { b in
                RoundedRectangle(cornerRadius: 3, style: .continuous)
                    .fill(LinearGradient(colors: [AppTheme.accent, AppTheme.chartCyan], startPoint: .top, endPoint: .bottom))
                    .frame(maxWidth: .infinity)
                    .frame(height: max(4, 90 * CGFloat(b.meters / maxM)))
                    .opacity(b.meters <= 0 ? 0.18 : 1)
            }
        }
        .frame(height: 100, alignment: .bottom)
    }
}

struct SimpleAreaChart: View {
    let values: [Double]
    var body: some View {
        GeometryReader { geo in
            let vals = values.isEmpty ? [0.0] : values
            let maxV = max(vals.max() ?? 1, 1)
            let step = geo.size.width / CGFloat(max(vals.count - 1, 1))
            Path { p in
                p.move(to: CGPoint(x: 0, y: geo.size.height))
                for (i, v) in vals.enumerated() {
                    let x = CGFloat(i) * step
                    let y = geo.size.height - CGFloat(v / maxV) * geo.size.height * 0.92
                    p.addLine(to: CGPoint(x: x, y: y))
                }
                p.addLine(to: CGPoint(x: geo.size.width, y: geo.size.height))
                p.closeSubpath()
            }
            .fill(LinearGradient(colors: [AppTheme.accent.opacity(0.35), AppTheme.chartCyan.opacity(0.05)], startPoint: .top, endPoint: .bottom))
            Path { p in
                for (i, v) in vals.enumerated() {
                    let x = CGFloat(i) * step
                    let y = geo.size.height - CGFloat(v / maxV) * geo.size.height * 0.92
                    if i == 0 { p.move(to: CGPoint(x: x, y: y)) }
                    else { p.addLine(to: CGPoint(x: x, y: y)) }
                }
            }
            .stroke(AppTheme.accent, style: StrokeStyle(lineWidth: 2, lineCap: .round, lineJoin: .round))
        }
        .frame(height: 96)
    }
}

struct HourHistogram: View {
    let bins: [Int]
    var body: some View {
        let maxV = max(bins.max() ?? 1, 1)
        HStack(alignment: .bottom, spacing: 2) {
            ForEach(0..<24, id: \.self) { h in
                RoundedRectangle(cornerRadius: 2)
                    .fill(AppTheme.chartCyan.opacity(0.85))
                    .frame(maxWidth: .infinity)
                    .frame(height: max(3, 64 * CGFloat(Double(bins[h]) / Double(maxV))))
                    .opacity(bins[h] == 0 ? 0.15 : 1)
            }
        }
        .frame(height: 72, alignment: .bottom)
    }
}

struct HeatmapGrid: View {
    let grid: [[Double]]
    var body: some View {
        let flat = grid.flatMap { $0 }
        let maxV = max(flat.max() ?? 1, 0.1)
        VStack(alignment: .leading, spacing: 3) {
            ForEach(Array(grid.enumerated()), id: \.offset) { _, row in
                HStack(spacing: 3) {
                    ForEach(Array(row.enumerated()), id: \.offset) { _, v in
                        RoundedRectangle(cornerRadius: 2, style: .continuous)
                            .fill(heatColor(v / maxV))
                            .frame(width: 12, height: 12)
                    }
                }
            }
        }
    }

    private func heatColor(_ t: Double) -> Color {
        if t <= 0 { return AppTheme.sunken }
        let x = min(max(t, 0), 1)
        return Color(
            red: 0.0 + (1 - x) * 0.75,
            green: 0.48 + (1 - x) * 0.25,
            blue: 1.0
        ).opacity(0.35 + 0.65 * x)
    }
}
