import Foundation
import CoreLocation

/// 行程统计口径：真实 GPS 导航记录；排除 mode 含「模拟」的条目（防历史脏数据）。
enum TripPeriod: String, CaseIterable, Identifiable {
    case day = "日"
    case week = "周"
    case month = "月"
    case year = "年"
    case all = "全部"
    var id: String { rawValue }

    var heroLabel: String {
        switch self {
        case .day: return "今日里程"
        case .week: return "本周里程"
        case .month: return "本月里程"
        case .year: return "今年里程"
        case .all: return "历史总里程"
        }
    }

    /// 总览标题区固定副标题（产品要求）。
    var overviewSubtitle: String { "行程概述" }
}

/// 城快 / 高速对比：仅凭 GPS 速度启发式；样本不足时 `identifiable == false`。
struct RoadClassSplit {
    let expressMeters: Double
    let highwayMeters: Double
    let otherMeters: Double
    let identifiable: Bool

    var totalClassified: Double { expressMeters + highwayMeters + otherMeters }
    var expressShare: Double {
        let t = max(expressMeters + highwayMeters, 1)
        return expressMeters / t
    }
    var highwayShare: Double {
        let t = max(expressMeters + highwayMeters, 1)
        return highwayMeters / t
    }
}

struct TripBucket: Identifiable {
    let id: String
    let label: String
    let meters: Double
    let seconds: TimeInterval
    let count: Int
    let date: Date?
}

struct TripInsight: Identifiable {
    let id = UUID()
    let icon: String
    let title: String
    let detail: String
}

struct TripStatsSnapshot {
    let period: TripPeriod
    let rides: [RideRecorder.Ride]
    let previousRides: [RideRecorder.Ride]

    var meters: Double { rides.reduce(0) { $0 + $1.meters } }
    var km: Double { meters / 1000.0 }
    var count: Int { rides.count }
    var duration: TimeInterval {
        rides.reduce(0) { $0 + Self.duration(of: $1) }
    }

    var previousMeters: Double { previousRides.reduce(0) { $0 + $1.meters } }
    var previousCount: Int { previousRides.count }

    var avgKmPerDay: Double {
        let days = max(1, Self.daySpan(for: period, rides: rides))
        return km / Double(days)
    }

    var avgDurationPerTrip: TimeInterval {
        guard count > 0 else { return 0 }
        return duration / Double(count)
    }

    var avgSpeedKmh: Double {
        let hours = max(duration / 3600.0, 1.0 / 3600.0)
        return km / hours
    }

    var maxSpeedKmh: Double {
        let mps = rides.flatMap(\.fixes).map(\.speed).max() ?? 0
        return max(0, mps) * 3.6
    }

    var longest: RideRecorder.Ride? {
        rides.max(by: { $0.meters < $1.meters })
    }

    var streakDays: Int {
        TripStatsEngine.drivingStreak(rides: TripStatsEngine.allRealRides(RideRecorder.shared.rides))
    }

    var periodDeltaPercent: Double? {
        guard previousMeters > 50 else { return nil }
        return ((meters - previousMeters) / previousMeters) * 100.0
    }

    var periodCountDelta: Int { count - previousCount }

    var barBuckets: [TripBucket] { TripStatsEngine.barBuckets(period: period, rides: rides) }

    var hourHistogram: [Int] {
        var bins = Array(repeating: 0, count: 24)
        let cal = Calendar.current
        for ride in rides {
            let h = cal.component(.hour, from: ride.started)
            bins[min(max(h, 0), 23)] += 1
        }
        return bins
    }

    var heatmap: [[Double]] {
        TripStatsEngine.heatmapGrid(rides: TripStatsEngine.allRealRides(RideRecorder.shared.rides), weeks: 16)
    }

    var weekdayShare: (work: Double, weekend: Double) {
        var work = 0.0, weekend = 0.0
        let cal = Calendar.current
        for ride in rides {
            let wd = cal.component(.weekday, from: ride.started)
            if wd == 1 || wd == 7 { weekend += ride.meters } else { work += ride.meters }
        }
        let total = max(work + weekend, 1)
        return (work / total, weekend / total)
    }

    var insights: [TripInsight] {
        guard count >= 3 else { return [] }
        var items: [TripInsight] = []
        if let day = TripStatsEngine.mostActiveWeekday(rides: rides) {
            items.append(TripInsight(
                icon: "日",
                title: "最活跃日 · \(day.name)",
                detail: String(format: "本区间累计 %.0f km", day.km)
            ))
        }
        if let slot = TripStatsEngine.peakHourRange(rides: rides) {
            items.append(TripInsight(
                icon: String(format: "%02d", slot.start),
                title: "最常时段 · \(String(format: "%02d:00-%02d:00", slot.start, slot.end))",
                detail: String(format: "约占全部行程 %.0f%%", slot.share * 100)
            ))
        }
        let avg = count > 0 ? km / Double(count) : 0
        items.append(TripInsight(
            icon: "均",
            title: String(format: "均次距离 %.1f km", avg),
            detail: "基于真实导航记录；模拟导航不计入"
        ))
        items.append(TripInsight(
            icon: "速",
            title: String(format: "均速 %.0f · 最高 %.0f km/h", avgSpeedKmh, maxSpeedKmh),
            detail: roadClassCaption
        ))
        return items
    }

    var roadClass: RoadClassSplit {
        TripStatsEngine.roadClassSplit(rides: rides)
    }

    var roadClassCaption: String {
        let split = roadClass
        guard split.identifiable else {
            return "城快 / 高速：数据不足，暂无法识别（需足够持续速度样本）"
        }
        return String(
            format: "城快约 %.0f%% · 高速约 %.0f%%（速度启发式，非路网官方等级）",
            split.expressShare * 100,
            split.highwayShare * 100
        )
    }

    var habitSummary: String {
        guard count >= 3 else { return "数据不足" }
        let share = weekdayShare
        let streak = streakDays
        let peakDay = TripStatsEngine.peakSingleDayKm(rides: TripStatsEngine.allRealRides(RideRecorder.shared.rides))
        return String(
            format: "工作日里程约占 %.0f%%，周末约占 %.0f%%。连续驾驶最长 %d 天；历史最高单日 %.1f km。",
            share.work * 100, share.weekend * 100, streak, peakDay
        )
    }

    var dataSufficientForInsights: Bool { count >= 3 }

    static func duration(of ride: RideRecorder.Ride) -> TimeInterval {
        let end = ride.ended ?? ride.fixes.last?.time ?? ride.started
        return max(0, end.timeIntervalSince(ride.started))
    }

    private static func daySpan(for period: TripPeriod, rides: [RideRecorder.Ride]) -> Int {
        let cal = Calendar.current
        switch period {
        case .day: return 1
        case .week: return 7
        case .month:
            let now = Date()
            return cal.range(of: .day, in: .month, for: now)?.count ?? 30
        case .year: return 365
        case .all:
            guard let first = rides.map(\.started).min(), let last = rides.map(\.started).max() else { return 1 }
            return max(1, (cal.dateComponents([.day], from: cal.startOfDay(for: first), to: cal.startOfDay(for: last)).day ?? 0) + 1)
        }
    }
}

enum TripStatsEngine {
    static func allRealRides(_ rides: [RideRecorder.Ride]) -> [RideRecorder.Ride] {
        rides.filter { !$0.mode.contains("模拟") }
    }

    static func snapshot(period: TripPeriod, from rides: [RideRecorder.Ride], now: Date = Date()) -> TripStatsSnapshot {
        let real = allRealRides(rides)
        let current = filter(real, period: period, now: now, previous: false)
        let previous = filter(real, period: period, now: now, previous: true)
        return TripStatsSnapshot(period: period, rides: current, previousRides: previous)
    }

    static func filter(_ rides: [RideRecorder.Ride], period: TripPeriod, now: Date, previous: Bool) -> [RideRecorder.Ride] {
        let cal = Calendar.current
        switch period {
        case .day:
            let day = previous ? cal.date(byAdding: .day, value: -1, to: now)! : now
            return rides.filter { cal.isDate($0.started, inSameDayAs: day) }
        case .week:
            let week = previous ? cal.date(byAdding: .weekOfYear, value: -1, to: now)! : now
            return rides.filter { cal.isDate($0.started, equalTo: week, toGranularity: .weekOfYear) }
        case .month:
            let month = previous ? cal.date(byAdding: .month, value: -1, to: now)! : now
            return rides.filter { cal.isDate($0.started, equalTo: month, toGranularity: .month) }
        case .year:
            let year = previous ? cal.date(byAdding: .year, value: -1, to: now)! : now
            return rides.filter { cal.isDate($0.started, equalTo: year, toGranularity: .year) }
        case .all:
            if previous { return [] }
            return rides
        }
    }

    static func barBuckets(period: TripPeriod, rides: [RideRecorder.Ride], now: Date = Date()) -> [TripBucket] {
        let cal = Calendar.current
        switch period {
        case .day:
            // 按小时聚合今日
            var map: [Int: (m: Double, s: TimeInterval, c: Int)] = [:]
            for ride in rides {
                let h = cal.component(.hour, from: ride.started)
                var cur = map[h] ?? (0, 0, 0)
                cur.m += ride.meters
                cur.s += TripStatsSnapshot.duration(of: ride)
                cur.c += 1
                map[h] = cur
            }
            return (0..<24).map { h in
                let v = map[h] ?? (0, 0, 0)
                return TripBucket(id: "h\(h)", label: "\(h)", meters: v.m, seconds: v.s, count: v.c, date: nil)
            }
        case .week:
            let labels = ["一", "二", "三", "四", "五", "六", "日"]
            // weekday: 1=Sun … 7=Sat → map to Mon-first index
            var map: [Int: (m: Double, s: TimeInterval, c: Int, d: Date?)] = [:]
            for ride in rides {
                let wd = cal.component(.weekday, from: ride.started)
                let idx = (wd + 5) % 7 // Mon=0
                var cur = map[idx] ?? (0, 0, 0, nil)
                cur.m += ride.meters
                cur.s += TripStatsSnapshot.duration(of: ride)
                cur.c += 1
                cur.d = ride.started
                map[idx] = cur
            }
            return (0..<7).map { i in
                let v = map[i] ?? (0, 0, 0, nil)
                return TripBucket(id: "w\(i)", label: labels[i], meters: v.m, seconds: v.s, count: v.c, date: v.d)
            }
        case .month:
            let days = cal.range(of: .day, in: .month, for: now)?.count ?? 30
            var map: [Int: (m: Double, s: TimeInterval, c: Int)] = [:]
            for ride in rides {
                let d = cal.component(.day, from: ride.started)
                var cur = map[d] ?? (0, 0, 0)
                cur.m += ride.meters
                cur.s += TripStatsSnapshot.duration(of: ride)
                cur.c += 1
                map[d] = cur
            }
            return (1...days).map { d in
                let v = map[d] ?? (0, 0, 0)
                return TripBucket(id: "d\(d)", label: "\(d)", meters: v.m, seconds: v.s, count: v.c, date: nil)
            }
        case .year:
            let labels = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"]
            var map: [Int: (m: Double, s: TimeInterval, c: Int)] = [:]
            for ride in rides {
                let m = cal.component(.month, from: ride.started)
                var cur = map[m] ?? (0, 0, 0)
                cur.m += ride.meters
                cur.s += TripStatsSnapshot.duration(of: ride)
                cur.c += 1
                map[m] = cur
            }
            return (1...12).map { m in
                let v = map[m] ?? (0, 0, 0)
                return TripBucket(id: "m\(m)", label: labels[m - 1], meters: v.m, seconds: v.s, count: v.c, date: nil)
            }
        case .all:
            // 按年
            let years = Set(rides.map { cal.component(.year, from: $0.started) }).sorted()
            if years.isEmpty {
                let y = cal.component(.year, from: now)
                return [TripBucket(id: "y\(y)", label: "\(y)", meters: 0, seconds: 0, count: 0, date: nil)]
            }
            return years.map { y in
                let subset = rides.filter { cal.component(.year, from: $0.started) == y }
                return TripBucket(
                    id: "y\(y)",
                    label: "\(y)",
                    meters: subset.reduce(0) { $0 + $1.meters },
                    seconds: subset.reduce(0) { $0 + TripStatsSnapshot.duration(of: $1) },
                    count: subset.count,
                    date: nil
                )
            }
        }
    }

    /// 16 周 × 7 天热力（周一起），值 = 当日里程 km。
    static func heatmapGrid(rides: [RideRecorder.Ride], weeks: Int, now: Date = Date()) -> [[Double]] {
        let cal = Calendar.current
        let today = cal.startOfDay(for: now)
        let weekday = cal.component(.weekday, from: today)
        let mondayOffset = (weekday + 5) % 7
        guard let thisMonday = cal.date(byAdding: .day, value: -mondayOffset, to: today),
              let start = cal.date(byAdding: .day, value: -(weeks - 1) * 7, to: thisMonday)
        else { return Array(repeating: Array(repeating: 0, count: 7), count: weeks) }

        var dayKm: [Date: Double] = [:]
        for ride in rides {
            let d = cal.startOfDay(for: ride.started)
            dayKm[d, default: 0] += ride.meters / 1000.0
        }
        var grid: [[Double]] = []
        for w in 0..<weeks {
            var row: [Double] = []
            for d in 0..<7 {
                let day = cal.date(byAdding: .day, value: w * 7 + d, to: start)!
                row.append(dayKm[day] ?? 0)
            }
            grid.append(row)
        }
        return grid
    }

    static func mostActiveWeekday(rides: [RideRecorder.Ride]) -> (name: String, km: Double)? {
        let names = ["日", "一", "二", "三", "四", "五", "六"]
        var map: [Int: Double] = [:]
        let cal = Calendar.current
        for ride in rides {
            let wd = cal.component(.weekday, from: ride.started)
            map[wd, default: 0] += ride.meters
        }
        guard let best = map.max(by: { $0.value < $1.value }) else { return nil }
        return (names[best.key - 1], best.value / 1000.0)
    }

    static func peakHourRange(rides: [RideRecorder.Ride]) -> (start: Int, end: Int, share: Double)? {
        guard !rides.isEmpty else { return nil }
        var bins = Array(repeating: 0, count: 24)
        let cal = Calendar.current
        for ride in rides {
            bins[cal.component(.hour, from: ride.started)] += 1
        }
        var bestStart = 0, bestSum = -1
        for h in 0..<23 {
            let s = bins[h] + bins[h + 1]
            if s > bestSum { bestSum = s; bestStart = h }
        }
        return (bestStart, bestStart + 2, Double(bestSum) / Double(rides.count))
    }

    static func drivingStreak(rides: [RideRecorder.Ride], now: Date = Date()) -> Int {
        let cal = Calendar.current
        let days = Set(rides.map { cal.startOfDay(for: $0.started) })
        guard !days.isEmpty else { return 0 }
        var streak = 0
        var cursor = cal.startOfDay(for: now)
        // 若今天无记录，从昨天起算连续
        if !days.contains(cursor) {
            cursor = cal.date(byAdding: .day, value: -1, to: cursor)!
        }
        while days.contains(cursor) {
            streak += 1
            cursor = cal.date(byAdding: .day, value: -1, to: cursor)!
        }
        return streak
    }

    static func peakSingleDayKm(rides: [RideRecorder.Ride]) -> Double {
        let cal = Calendar.current
        var map: [Date: Double] = [:]
        for ride in rides {
            map[cal.startOfDay(for: ride.started), default: 0] += ride.meters
        }
        return (map.values.max() ?? 0) / 1000.0
    }

    static func topRoutes(rides: [RideRecorder.Ride], limit: Int = 10) -> [(title: String, count: Int, avgKm: Double)] {
        // MVP：按 mode 名聚合；无自定义起终点地名时用模式/距离桶
        var map: [String: (c: Int, m: Double)] = [:]
        for ride in rides {
            let key: String
            if ride.mode.isEmpty {
                let bucket = Int((ride.meters / 1000.0).rounded())
                key = bucket <= 0 ? "短途导航" : "约 \(bucket) km 路线"
            } else {
                key = ride.mode
            }
            var cur = map[key] ?? (0, 0)
            cur.c += 1
            cur.m += ride.meters
            map[key] = cur
        }
        return map.map { (title: $0.key, count: $0.value.c, avgKm: $0.value.m / 1000.0 / Double(max(1, $0.value.c))) }
            .sorted { $0.count > $1.count }
            .prefix(limit)
            .map { $0 }
    }

    /// GPS 速度启发式：≥80 km/h 持续段计高速，50–80 计城快；分类里程不足 2 km 或占比过低 → 不可识别。
    static func roadClassSplit(rides: [RideRecorder.Ride]) -> RoadClassSplit {
        var express = 0.0, highway = 0.0, other = 0.0
        for ride in rides {
            let fixes = ride.fixes
            guard fixes.count >= 2 else { continue }
            for i in 1..<fixes.count {
                let a = fixes[i - 1], b = fixes[i]
                let dist = CLLocation(latitude: a.lat, longitude: a.lng)
                    .distance(from: CLLocation(latitude: b.lat, longitude: b.lng))
                guard dist > 0, dist < 400 else { continue }
                let spA = max(0, a.speed) * 3.6
                let spB = max(0, b.speed) * 3.6
                // 无效 GPS 速度（负值已夹 0）时用位移/时间估算
                var kmh = (spA + spB) / 2
                if kmh < 1 {
                    let dt = b.time.timeIntervalSince(a.time)
                    if dt > 0.5 { kmh = (dist / dt) * 3.6 }
                }
                if kmh >= 80 { highway += dist }
                else if kmh >= 50 { express += dist }
                else { other += dist }
            }
        }
        let classified = express + highway
        let identifiable = classified >= 2000 && classified / max(classified + other, 1) >= 0.12
        return RoadClassSplit(
            expressMeters: express,
            highwayMeters: highway,
            otherMeters: other,
            identifiable: identifiable
        )
    }

    static func rides(on day: Date, from rides: [RideRecorder.Ride]) -> [RideRecorder.Ride] {
        let cal = Calendar.current
        return allRealRides(rides).filter { cal.isDate($0.started, inSameDayAs: day) }
    }

    static func annual(rides: [RideRecorder.Ride], year: Int? = nil, now: Date = Date()) -> (
        year: Int, km: Double, hours: Double, count: Int, avgSpeed: Double, longestKm: Double, topMode: String, activeMonth: Int
    ) {
        let cal = Calendar.current
        let y = year ?? cal.component(.year, from: now)
        let subset = allRealRides(rides).filter { cal.component(.year, from: $0.started) == y }
        let meters = subset.reduce(0.0) { $0 + $1.meters }
        let seconds = subset.reduce(0.0) { $0 + TripStatsSnapshot.duration(of: $1) }
        let hours = seconds / 3600.0
        let avg = hours > 0 ? (meters / 1000.0) / hours : 0
        let longest = (subset.map(\.meters).max() ?? 0) / 1000.0
        let top = topRoutes(rides: subset, limit: 1).first?.title ?? "—"
        var monthMap: [Int: Double] = [:]
        for ride in subset {
            monthMap[cal.component(.month, from: ride.started), default: 0] += ride.meters
        }
        let active = monthMap.max(by: { $0.value < $1.value })?.key ?? 0
        return (y, meters / 1000.0, hours, subset.count, avg, longest, top, active)
    }
}

enum TripFormat {
    static func km(_ meters: Double) -> String {
        let km = meters / 1000.0
        if km >= 100 { return String(format: "%.0f", km) }
        if km >= 10 { return String(format: "%.1f", km) }
        return String(format: "%.1f", km)
    }

    static func kmUnit(_ meters: Double) -> String { "\(km(meters)) km" }

    static func duration(_ seconds: TimeInterval) -> String {
        let s = max(0, Int(seconds))
        let h = s / 3600, m = (s % 3600) / 60
        if h > 0 { return String(format: "%dh %02dm", h, m) }
        return String(format: "%dm", m)
    }

    static func speed(_ kmh: Double) -> String {
        String(format: "%.1f km/h", kmh)
    }

    static func percent(_ v: Double) -> String {
        let sign = v >= 0 ? "+" : ""
        return String(format: "%@%.0f%%", sign, v)
    }

    static func relativeDay(_ date: Date) -> String {
        let cal = Calendar.current
        if cal.isDateInToday(date) { return "今天" }
        if cal.isDateInYesterday(date) { return "昨天" }
        let f = DateFormatter(); f.dateFormat = "M月d日"
        return f.string(from: date)
    }
}
