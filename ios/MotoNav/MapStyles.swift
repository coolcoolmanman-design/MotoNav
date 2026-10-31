import Foundation

enum MapStyles {
    static func original(_ raw: [UInt32]) -> (indices: [UInt8], palette: [Int]) {
        var counts: [Int: Int] = [:]
        for c in raw { let rgb = Int((c >> 19) & 31) << 11 | Int((c >> 10) & 63) << 5 | Int((c >> 3) & 31); counts[rgb, default: 0] += 1 }
        let ranked = counts.keys.sorted { counts[$0]! == counts[$1]! ? $0 < $1 : counts[$0]! > counts[$1]! }
        var palette = Array(ranked.prefix(16)); while palette.count < 16 { palette.append(palette.last ?? 0) }
        func delta(_ a: Int, _ b: Int) -> Int {
            let r = (a >> 11) - (b >> 11), g = ((a >> 5) & 63) - ((b >> 5) & 63), bl = (a & 31) - (b & 31)
            return r*r*4 + g*g + bl*bl*4
        }
        var lookup: [Int: UInt8] = [:]
        for c in ranked { lookup[c] = UInt8(palette.indices.min { delta(c, palette[$0]) < delta(c, palette[$1]) }!) }
        return (raw.map { c in lookup[Int((c >> 19) & 31) << 11 | Int((c >> 10) & 63) << 5 | Int((c >> 3) & 31)]! }, palette)
    }
    static func minimal(_ raw: [UInt32], size: Int) -> [UInt8] {
        // Share the measured-bg classifier so highway mid-gray pavement is not dropped
        // (white-only / yellow-only masks produced the blank cream basemap in the field).
        return MapGeo.classifyAmapBase(raw, w: size, h: size).map { $0 == 1 ? 1 : 0 }
    }
}
