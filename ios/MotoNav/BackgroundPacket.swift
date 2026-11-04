import Foundation

/// 自定义开机背景 bulk type 8 wire helpers（与固件 `parseBg` / musicA.17 对齐）。
/// ver=1：'BGR' 1 w_le16 h_le16 + zlib(RGB565 LE)。w=h=0 = 清除（固件回 faceId 表盘背景）。
/// 固件不持久化：每次开机连上 App 后经 CAP,BGR,1 触发自动重推；掉电回默认。
enum BackgroundPacket {
    static let bulkType = 8
    static let px = 466
    static let headerBytes = 8

    static func minPayloadBytes(w: Int, h: Int) -> Int {
        headerBytes + (w > 0 && h > 0 ? 8 : 0)
    }

    /// 全屏 466×466 背景包。
    static func encode(w: Int, h: Int, rgb565LE: [UInt8]) -> Data? {
        guard w > 0, h > 0, w <= px, h <= px, rgb565LE.count == w * h * 2 else { return nil }
        var d = Data([
            UInt8(ascii: "B"), UInt8(ascii: "G"), UInt8(ascii: "R"), 1,
            UInt8(w & 0xFF), UInt8((w >> 8) & 0xFF),
            UInt8(h & 0xFF), UInt8((h >> 8) & 0xFF)
        ])
        d.append(BadgePacket.zlibForFirmware(rgb565LE))
        guard d.count >= minPayloadBytes(w: w, h: h) else { return nil }
        return d
    }

    /// 清除：8 字节头，w=h=0 → 固件恢复默认背景。
    static func clear() -> Data {
        Data([
            UInt8(ascii: "B"), UInt8(ascii: "G"), UInt8(ascii: "R"), 1,
            0, 0, 0, 0
        ])
    }
}
