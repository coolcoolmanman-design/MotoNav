import Foundation

var checks = 0
func check(_ condition: @autoclosure () -> Bool, _ label: String) {
    precondition(condition(), label); checks += 1
}

let clear = CoverPacket.clear()
check([UInt8](clear) == [UInt8(ascii: "C"), UInt8(ascii: "V"), UInt8(ascii: "R"), 1, 0, 0], "Clear cover header")

let raw = [UInt8](repeating: 0xAB, count: CoverPacket.px * CoverPacket.px * 2)
guard let packet = CoverPacket.encode(w: CoverPacket.px, h: CoverPacket.px, rgb565LE: raw) else {
    fatalError("encode failed")
}
let bytes = [UInt8](packet)
check(bytes[0] == UInt8(ascii: "C") && bytes[1] == UInt8(ascii: "V") && bytes[2] == UInt8(ascii: "R") && bytes[3] == 1, "CVR magic")
check(bytes[4] == UInt8(CoverPacket.px) && bytes[5] == UInt8(CoverPacket.px), "px×px dimensions")
check(packet.count >= CoverPacket.minPayloadBytes(w: CoverPacket.px, h: CoverPacket.px), "Passes firmware len>=6+need gate")
check(bytes[6] == 0x78 && bytes[7] == 0x9C, "zlib CMF/FLG")
// Store block: BFINAL/BTYPE — 240² RGB565 spans multiple 64KiB store chunks.
check(bytes[8] == 0x00 || bytes[8] == 0x01, "zlib store block header")
let len = Int(bytes[9]) | Int(bytes[10]) << 8
check(len == min(65535, raw.count), "first store LEN")
check(CoverPacket.bulkType == 5, "bulk type 5")
check(CoverPacket.px == 240, "musicA.2 CVR side 240")
// Compressed-sized fake must fail the gate helper (documents the FW quirk we work around)
let tiny = CoverPacket.header(w: CoverPacket.px, h: CoverPacket.px, data: Data(repeating: 1, count: 100))
check(tiny.count < CoverPacket.minPayloadBytes(w: CoverPacket.px, h: CoverPacket.px), "deflate-sized payload would fail FW gate")
print("Cover packet: \(checks) checks passed; wire \(packet.count) bytes")
