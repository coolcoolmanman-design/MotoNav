import Foundation

/// 中央组合精灵 bulk type 11（musicA.50/52 / 与固件 `parseCsp` 对齐）。
/// ver=2：'CSP' 2 role frameCount w_le16 h_le16 + 每帧 u32 zlen + zlib(RGB565 LE 466²)。
/// role 1=sleep 2=chase；w=h=0 清除该 role（role=0 清两套）。
enum CenterSpritePacket {
    static let bulkType = 11
    static let px = 466
    static let headerBytes = 10
    static let roleSleep = 1
    static let roleChase = 2
    static let roleClearAll = 0

    static func encode(role: Int, frames: [[UInt8]]) -> Data? {
        guard role == roleSleep || role == roleChase else { return nil }
        guard !frames.isEmpty, frames.count <= 12 else { return nil }
        let need = px * px * 2
        for f in frames where f.count != need { return nil }
        var d = Data([
            UInt8(ascii: "C"), UInt8(ascii: "S"), UInt8(ascii: "P"), 2,
            UInt8(role & 0xFF), UInt8(frames.count & 0xFF),
            UInt8(px & 0xFF), UInt8((px >> 8) & 0xFF),
            UInt8(px & 0xFF), UInt8((px >> 8) & 0xFF)
        ])
        for raw in frames {
            let z = BadgePacket.zlibForFirmware(raw)
            var zlen = UInt32(z.count).littleEndian
            withUnsafeBytes(of: &zlen) { d.append(contentsOf: $0) }
            d.append(z)
        }
        return d
    }

    /// 清除：role 0=两套；1/2=单套。
    static func clear(role: Int = roleClearAll) -> Data {
        Data([
            UInt8(ascii: "C"), UInt8(ascii: "S"), UInt8(ascii: "P"), 2,
            UInt8(role & 0xFF), 0,
            0, 0, 0, 0
        ])
    }
}
