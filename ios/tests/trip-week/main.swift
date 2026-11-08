import Foundation

var checks = 0
func check(_ condition: Bool, _ message: String) {
    checks += 1
    precondition(condition, message)
}

func day(_ y: Int, _ m: Int, _ d: Int, cal: Calendar) -> Date {
    var c = DateComponents()
    c.year = y; c.month = m; c.day = d; c.hour = 12
    return cal.date(from: c)!
}

func ride(_ date: Date, meters: Double = 1000, mode: String = "导航") -> RideRecorder.Ride {
    RideRecorder.Ride(started: date, ended: date.addingTimeInterval(600), mode: mode, fixes: [], meters: meters)
}

// Sunday 2026-11-08 — reported bug day. en_US firstWeekday=Sunday reproduces
// weekOfYear collapsing「本周」to today; monday-aligned filter keeps Mon–Sun.
var us = Calendar(identifier: .gregorian)
us.locale = Locale(identifier: "en_US")
us.timeZone = TimeZone(identifier: "Asia/Shanghai")!
check(us.firstWeekday == 1, "en_US firstWeekday should be Sunday")

let sunday = day(2026, 11, 8, cal: us)
let monday = day(2026, 11, 2, cal: us)
let saturday = day(2026, 11, 7, cal: us)
let prevSunday = day(2026, 11, 1, cal: us)
let nextMonday = day(2026, 11, 9, cal: us)

let interval = TripStatsEngine.mondayWeekInterval(containing: sunday, previous: false, calendar: us)!
check(us.isDate(interval.start, inSameDayAs: monday), "week start is Monday 2026-11-02")
check(us.isDate(interval.end, inSameDayAs: nextMonday), "week end is next Monday 00:00")

let localeSameWeekMon = us.isDate(monday, equalTo: sunday, toGranularity: .weekOfYear)
let localeSameWeekSat = us.isDate(saturday, equalTo: sunday, toGranularity: .weekOfYear)
check(!localeSameWeekMon && !localeSameWeekSat, "locale weekOfYear on Sun-first excludes Mon–Sat from Sunday's week")
check(us.isDate(sunday, equalTo: sunday, toGranularity: .weekOfYear), "locale weekOfYear still matches Sunday")

let synth = [
    ride(prevSunday, meters: 100),
    ride(monday, meters: 1000),
    ride(day(2026, 11, 3, cal: us), meters: 2000),
    ride(day(2026, 11, 4, cal: us), meters: 3000),
    ride(day(2026, 11, 5, cal: us), meters: 4000),
    ride(day(2026, 11, 6, cal: us), meters: 5000),
    ride(saturday, meters: 6000),
    ride(sunday, meters: 7000),
    ride(nextMonday, meters: 8000),
]

let filtered = TripStatsEngine.filter(synth, period: .week, now: sunday, previous: false, calendar: us)
check(filtered.count == 7, "filter keeps Mon–Sun under Sun-first calendar (got \(filtered.count))")
check(filtered.map(\.meters).reduce(0, +) == 28000, "week meters sum Mon–Sun")
check(!filtered.contains(where: { us.isDate($0.started, inSameDayAs: prevSunday) }), "prev Sunday excluded")
check(!filtered.contains(where: { us.isDate($0.started, inSameDayAs: nextMonday) }), "next Monday excluded")

// Old weekOfYear semantics on the same calendar would keep only Sunday.
let legacyOnlyToday = synth.filter { us.isDate($0.started, equalTo: sunday, toGranularity: .weekOfYear) }
check(legacyOnlyToday.count == 1 && us.isDate(legacyOnlyToday[0].started, inSameDayAs: sunday),
      "legacy weekOfYear reproduces only-today bug")

let buckets = TripStatsEngine.barBuckets(period: .week, rides: filtered, now: sunday)
check(buckets.count == 7, "week bars always 7")
check(buckets.map(\.label) == ["一", "二", "三", "四", "五", "六", "日"], "Mon-first labels")
for (i, expected) in [1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0].enumerated() {
    check(abs(buckets[i].meters - expected) < 0.1, "bucket \(i) meters \(buckets[i].meters) != \(expected)")
}
check(buckets.filter { $0.meters > 0 }.count == 7, "all seven weekday bars painted for synthetic Mon–Sun data")

let prev = TripStatsEngine.filter(synth, period: .week, now: sunday, previous: true, calendar: us)
check(prev.count == 1 && abs(prev[0].meters - 100) < 0.1, "previous week is prior Sunday only in fixture")

var cn = Calendar(identifier: .gregorian)
cn.locale = Locale(identifier: "zh_CN")
cn.timeZone = TimeZone(identifier: "Asia/Shanghai")!
let cnFiltered = TripStatsEngine.filter(synth, period: .week, now: sunday, previous: false, calendar: cn)
check(cnFiltered.map(\.meters) == filtered.map(\.meters), "zh_CN matches en_US under explicit Mon–Sun bounds")

print("trip-week ok (\(checks) checks)")
