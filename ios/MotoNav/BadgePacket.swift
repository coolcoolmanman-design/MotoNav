import Foundation
import Compression

/// 电子吧唧 bulk type 7 wire helpers（与固件 `parseBadge` / 7.12.11-opt.11 对齐）。
/// 载荷：'B''D''G' ver=1 w_le16 h_le16 + zlib(RGB565 LE)。w/h 0 清除。
/// 原生全屏 466×466（非 CoverBridge 128 / 非 240·320）。
/// type 7：避开上游安卓 `CoverPacket.TYPE_COVER = 6`；本侧音乐封面仍为 type 5。
enum BadgePacket {
    static let bulkType = 7
    static let px = 466
    static let headerBytes = 8

    static func minPayloadBytes(w: Int, h: Int) -> Int {
        headerBytes + (w > 0 && h > 0 ? 8 : 0)
    }

    static func header(w: Int, h: Int, data: Data) -> Data {
        var d = Data([
            UInt8(ascii: "B"), UInt8(ascii: "D"), UInt8(ascii: "G"), 1,
            UInt8(w & 0xFF), UInt8((w >> 8) & 0xFF),
            UInt8(h & 0xFF), UInt8((h >> 8) & 0xFF)
        ])
        d.append(data)
        return d
    }

    static func clear() -> Data { header(w: 0, h: 0, data: Data()) }

    /// 真 Deflate + zlib 头尾；失败时退回 store-block（仍合法 zlib）。
    static func zlibForFirmware(_ raw: [UInt8]) -> Data {
        var out = Data([0x78, 0x9C])
        if let deflated = try? (Data(raw) as NSData).compressed(using: .zlib) as Data, !deflated.isEmpty {
            out.append(deflated)
        } else {
            out.append(contentsOf: storeBlocks(raw))
        }
        var a: UInt32 = 1, b: UInt32 = 0
        for byte in raw { a = (a + UInt32(byte)) % 65521; b = (b + a) % 65521 }
        let sum = (b << 16) | a
        out.append(UInt8((sum >> 24) & 0xFF))
        out.append(UInt8((sum >> 16) & 0xFF))
        out.append(UInt8((sum >> 8) & 0xFF))
        out.append(UInt8(sum & 0xFF))
        return out
    }

    private static func storeBlocks(_ raw: [UInt8]) -> [UInt8] {
        var out: [UInt8] = []
        var offset = 0
        let n = raw.count
        if n == 0 {
            return [0x01, 0x00, 0x00, 0xFF, 0xFF]
        }
        while offset < n {
            let chunk = min(65535, n - offset)
            let last = offset + chunk >= n
            out.append(last ? 0x01 : 0x00)
            out.append(UInt8(chunk & 0xFF)); out.append(UInt8((chunk >> 8) & 0xFF))
            out.append(UInt8(~chunk & 0xFF)); out.append(UInt8((~chunk >> 8) & 0xFF))
            out.append(contentsOf: raw[offset..<(offset + chunk)])
            offset += chunk
        }
        return out
    }

    static func encode(w: Int, h: Int, rgb565LE: [UInt8]) -> Data? {
        guard w > 0, h > 0, w <= px, h <= px, rgb565LE.count == w * h * 2 else { return nil }
        let packet = header(w: w, h: h, data: zlibForFirmware(rgb565LE))
        guard packet.count >= minPayloadBytes(w: w, h: h) else { return nil }
        return packet
    }
}
