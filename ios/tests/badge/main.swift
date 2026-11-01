import Foundation

var checks = 0
func check(_ condition: @autoclosure () -> Bool, _ label: String) {
    precondition(condition(), label); checks += 1
}

let clear = BadgePacket.clear()
let cb = [UInt8](clear)
check(cb.count == 8, "Clear header 8 bytes")
check(cb[0] == UInt8(ascii: "B") && cb[1] == UInt8(ascii: "D") && cb[2] == UInt8(ascii: "G") && cb[3] == 1, "BDG magic")
check(cb[4] == 0 && cb[5] == 0 && cb[6] == 0 && cb[7] == 0, "Clear dims")

let raw = [UInt8](repeating: 0x55, count: BadgePacket.px * BadgePacket.px * 2)
guard let packet = BadgePacket.encode(w: BadgePacket.px, h: BadgePacket.px, rgb565LE: raw) else {
    fatalError("encode failed")
}
let bytes = [UInt8](packet)
check(bytes[0] == UInt8(ascii: "B") && bytes[1] == UInt8(ascii: "D") && bytes[2] == UInt8(ascii: "G"), "BDG")
check(Int(bytes[4]) | Int(bytes[5]) << 8 == 466, "width 466")
check(Int(bytes[6]) | Int(bytes[7]) << 8 == 466, "height 466")
check(bytes[8] == 0x78 && bytes[9] == 0x9C, "zlib CMF/FLG")
check(BadgePacket.bulkType == 7, "bulk type 7 (not upstream cover 6)")
check(packet.count >= BadgePacket.minPayloadBytes(w: 466, h: 466), "min payload")
print("Badge packet: \(checks) checks passed; wire \(packet.count) bytes; type \(BadgePacket.bulkType)")
