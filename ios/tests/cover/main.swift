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
check(bytes[4] == 128 && bytes[5] == 128, "128x128 dimensions")
check(packet.count >= CoverPacket.minPayloadBytes(w: 128, h: 128), "Passes firmware len>=6+need gate")
check(bytes[6] == 0x78 && bytes[7] == 0x9C, "zlib CMF/FLG")
// Store block: BFINAL=1 BTYPE=00, then LEN/NLEN for first 65535 (here 32768) bytes
check(bytes[8] == 0x01, "zlib store BFINAL")
let len = Int(bytes[9]) | Int(bytes[10]) << 8
check(len == raw.count, "store LEN matches RGB565 bytes")
check(CoverPacket.bulkType == 5, "bulk type 5")
// Compressed-sized fake must fail the gate helper (documents the FW quirk we work around)
let tiny = CoverPacket.header(w: 128, h: 128, data: Data(repeating: 1, count: 100))
check(tiny.count < CoverPacket.minPayloadBytes(w: 128, h: 128), "deflate-sized payload would fail FW gate")
print("Cover packet: \(checks) checks passed; wire \(packet.count) bytes")
