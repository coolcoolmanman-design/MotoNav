import Foundation

/// Subset of boards/175 `CommandReceiver.handleText` relevant to MVP pages.
enum CommandParser {
    static func apply(_ raw: String, to state: SimState) {
        let m = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !m.isEmpty else { return }
        state.lastCommand = m
        state.log("← \(m)")

        if m == "P" { return }
        if m == "C" {
            state.stopArrival()
            state.resetNav()
            state.page = .idle
            return
        }
        if m.hasPrefix("#v,") {
            state.speedKmh = Int(m.dropFirst(3)) ?? state.speedKmh
            state.page = .nav
            return
        }
        if m.hasPrefix("#e") {
            // #e,remaining,etaHHMM,lights
            let body = String(m.dropFirst(2))
            let parts = body.split(separator: ",").map(String.init)
            if parts.count >= 1 { state.remainM = Int(parts[0]) ?? state.remainM }
            if parts.count >= 2 { state.etaHHMM = parts[1] }
            state.page = .nav
            return
        }
        if m.hasPrefix("#O") {
            // #O米/分钟/最高车速
            let body = String(m.dropFirst(2))
            let parts = body.split(separator: "/").map(String.init)
            if parts.count >= 2 {
                state.tripM = Int(parts[0]) ?? -1
                state.tripMin = Int(parts[1]) ?? -1
                state.tripMaxKmh = parts.count >= 3 ? (Int(parts[2]) ?? -1) : -1
            }
            return
        }
        if m.hasPrefix("#Z,") {
            let parts = m.split(separator: ",").map(String.init)
            if parts.count >= 3 {
                state.musicHave = (Int(parts[1]) ?? 0) != 0
                state.musicPlaying = (Int(parts[2]) ?? 0) != 0
            }
            state.page = .music
            return
        }
        if m.hasPrefix("#RC,") {
            state.coverSpin = (Int(m.dropFirst(4)) ?? 0) != 0
            return
        }
        if m.hasPrefix("#m,") {
            state.mapOn = (Int(m.dropFirst(3)) ?? 0) != 0
            return
        }
        if m.hasPrefix("#n,") {
            state.nightOn = (Int(m.dropFirst(3)) ?? 0) != 0
            return
        }
        if m.hasPrefix("#b,") {
            state.brightness = Int(m.dropFirst(3)) ?? state.brightness
            return
        }
        if m.hasPrefix("#x,") {
            state.gpsWeak = (Int(m.dropFirst(3)) ?? 0) != 0
            return
        }
        if m.hasPrefix("#y,") {
            state.rerouting = (Int(m.dropFirst(3)) ?? 0) != 0
            return
        }
        if m.hasPrefix("#p,") {
            // pose: keep on nav if map on
            if state.mapOn || state.inNav { state.page = .nav }
            return
        }
        if m == "#J" {
            state.startArrival(waypoint: true)
            return
        }
        if m.hasPrefix("#") { return } // unknown #… ignored like FW

        // Navigation: DIR,dist[,angle]
        if let comma = m.firstIndex(of: ",") {
            let dir = String(m[..<comma])
            let rest = String(m[m.index(after: comma)...])
            let prev = state.navDir
            state.navDir = dir
            if let c2 = rest.firstIndex(of: ",") {
                state.navDist = Int(rest[..<c2]) ?? 0
            } else {
                state.navDist = Int(rest) ?? 0
            }
            state.inNav = true
            if dir == "A", !prev.isEmpty, prev != "A" {
                state.startArrival(waypoint: false)
            } else if prev == "A", dir != "A" {
                state.stopArrival()
            } else if dir != "A", state.arrivalOn {
                state.stopArrival()
            }
            if dir != "A" { state.page = .nav }
        }
    }
}
