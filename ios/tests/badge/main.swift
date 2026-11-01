import Foundation

var checks = 0
func check(_ condition: @autoclosure () -> Bool, _ label: String) {
    precondition(condition(), label); checks += 1
}

// ver=1 clear（兼容 opt.11）
let clear = BadgePacket.clear()
let cb = [UInt8](clear)
check(cb.count == 8, "Clear header 8 bytes")
check(cb[0] == UInt8(ascii: "B") && cb[1] == UInt8(ascii: "D") && cb[2] == UInt8(ascii: "G") && cb[3] == 1, "BDG magic")
check(cb[4] == 0 && cb[5] == 0 && cb[6] == 0 && cb[7] == 0, "Clear dims")

// ver=2 clear-all
let clearAll = BadgePacket.clearAll()
let ca = [UInt8](clearAll)
check(ca.count == 10, "ClearAll header 10 bytes")
check(ca[3] == 2 && ca[4] == 0xFF && ca[5] == 0, "ver2 clear-all slot/count")
check(ca[6] == 0 && ca[7] == 0 && ca[8] == 0 && ca[9] == 0, "ClearAll dims")

let raw = [UInt8](repeating: 0x55, count: BadgePacket.px * BadgePacket.px * 2)
guard let packet = BadgePacket.encode(slot: 0, count: 1, w: BadgePacket.px, h: BadgePacket.px, rgb565LE: raw) else {
    fatalError("encode failed")
}
let bytes = [UInt8](packet)
check(bytes[0] == UInt8(ascii: "B") && bytes[1] == UInt8(ascii: "D") && bytes[2] == UInt8(ascii: "G"), "BDG")
check(bytes[3] == 2, "ver 2")
check(bytes[4] == 0 && bytes[5] == 1, "slot 0 count 1")
check(Int(bytes[6]) | Int(bytes[7]) << 8 == 466, "width 466")
check(Int(bytes[8]) | Int(bytes[9]) << 8 == 466, "height 466")
check(bytes[10] == 0x78 && bytes[11] == 0x9C, "zlib CMF/FLG")
check(BadgePacket.bulkType == 7, "bulk type 7 (not upstream cover 6)")
check(BadgePacket.maxSlots == 3, "max 3 slots")
check(packet.count >= BadgePacket.minPayloadBytes(w: 466, h: 466, ver: 2), "min payload")

// 单图便捷 encode 仍走 ver=2
guard let single = BadgePacket.encode(w: BadgePacket.px, h: BadgePacket.px, rgb565LE: raw) else {
    fatalError("single encode failed")
}
check([UInt8](single)[3] == 2, "single encode ver 2")

print("Badge packet: \(checks) checks passed; wire \(packet.count) bytes; type \(BadgePacket.bulkType)")
