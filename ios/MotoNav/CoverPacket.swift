import Foundation

/// Album-cover bulk type 5 wire helpers (shared with host tests).
/// Firmware (`parseCover`, 7.12.10-cover.1 / 7.12.11-opt.1):
///   'C''V''R' ver=1  w  h  + zlib(RGB565 LE). w/h 0 clears.
enum CoverPacket {
    static let bulkType = 5
    static let px = 128
    /// Must match firmware `COVER_MAX` / `parseCover` floor: `len >= 6 + w*h*2`.
    static func minPayloadBytes(w: Int, h: Int) -> Int { 6 + max(0, w) * max(0, h) * 2 }

    static func header(w: Int, h: Int, data: Data) -> Data {
        var d = Data([UInt8(ascii: "C"), UInt8(ascii: "V"), UInt8(ascii: "R"), 1, UInt8(w & 0xFF), UInt8(h & 0xFF)])
        d.append(data)
        return d
    }

    static func clear() -> Data { header(w: 0, h: 0, data: Data()) }

    /// RGB565 little-endian → zlib stream that passes the current firmware length gate.
    ///
    /// Root cause of “实测没有生效”: `parseCover` does `if (len < 6 + need) return` with
    /// `need = w*h*2`. Real Deflate output is almost always *smaller* than raw, so the
    /// gate rejects valid compressed covers before `tinfl` runs. Store-block zlib keeps
    /// a legal RFC1950 stream while satisfying `len >= 6+need` (fits `COVER_MAX` +64).
    /// Firmware follow-up: change that check to a small minimum (e.g. `len < 6+8`).
    static func zlibForFirmware(_ raw: [UInt8]) -> Data { zlibStore(raw) }

    /// zlib (CMF/FLG) + one or more uncompressed Deflate store blocks + Adler-32.
    static func zlibStore(_ raw: [UInt8]) -> Data {
        var out = Data([0x78, 0x9C])
        var offset = 0
        let n = raw.count
        if n == 0 {
            out.append(contentsOf: [0x01, 0x00, 0x00, 0xFF, 0xFF])
        } else {
            while offset < n {
                let chunk = min(65535, n - offset)
                let last = offset + chunk >= n
                out.append(last ? 0x01 : 0x00) // BFINAL | BTYPE=00
                out.append(UInt8(chunk & 0xFF)); out.append(UInt8((chunk >> 8) & 0xFF))
                out.append(UInt8(~chunk & 0xFF)); out.append(UInt8((~chunk >> 8) & 0xFF))
                out.append(contentsOf: raw[offset..<(offset + chunk)])
                offset += chunk
            }
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

    /// Encode full type-5 payload from LE RGB565 (`rgb565LE.count == w*h*2`).
    static func encode(w: Int, h: Int, rgb565LE: [UInt8]) -> Data? {
        guard w > 0, h > 0, rgb565LE.count == w * h * 2 else { return nil }
        let packet = header(w: w, h: h, data: zlibForFirmware(rgb565LE))
        guard packet.count >= minPayloadBytes(w: w, h: h) else { return nil }
        return packet
    }
}
