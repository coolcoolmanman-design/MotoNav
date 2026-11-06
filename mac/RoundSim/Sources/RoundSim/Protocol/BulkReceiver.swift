import Foundation

/// Firmware `BulkReceiver.h` subset: reassembles `'M'` packets and ACKs with `K,type,id`.
final class BulkReceiver {
    struct Slot {
        var id = -1
        var total: UInt32 = 0
        var parts = 0
        var got = 0
        var done = false
        var mask = [UInt8](repeating: 0, count: 512)
        var buf = Data()
    }

    private var slots: [Int: Slot] = [:]
    var onComplete: ((Int, Int, Data) -> Void)?
    var onAck: ((String) -> Void)?

    func handle(_ d: Data) {
        guard d.count >= 15, d[0] == UInt8(ascii: "M") else { return }
        let type = Int(d[1]), id = Int(d[2])
        let parts = Int(d[3]) | Int(d[4]) << 8
        let seq = Int(d[5]) | Int(d[6]) << 8
        let total = UInt32(d[7]) | UInt32(d[8]) << 8 | UInt32(d[9]) << 16 | UInt32(d[10]) << 24
        let off = UInt32(d[11]) | UInt32(d[12]) << 8 | UInt32(d[13]) << 16 | UInt32(d[14]) << 24
        guard (1...5).contains(type), total > 0, total <= 2_000_000, parts > 0, parts <= 4096, seq < parts else { return }

        var b = slots[type] ?? Slot()
        if id != b.id || total != b.total || parts != b.parts {
            b = Slot()
            b.id = id; b.total = total; b.parts = parts
            b.buf = Data(count: Int(total))
            b.mask = [UInt8](repeating: 0, count: max(512, (parts + 7) / 8))
        }
        if b.done {
            onAck?("K,\(type),\(id)")
            slots[type] = b
            return
        }
        var len = d.count - 15
        if off >= total { slots[type] = b; return }
        if off + UInt32(len) > total { len = Int(total - off) }
        let bit = 1 << (seq & 7)
        if (b.mask[seq >> 3] & UInt8(bit)) == 0 {
            let range = Int(off)..<(Int(off) + len)
            b.buf.replaceSubrange(range, with: d.subdata(in: 15..<(15 + len)))
            b.mask[seq >> 3] |= UInt8(bit)
            b.got += 1
        }
        if b.got >= b.parts {
            b.done = true
            onComplete?(type, id, b.buf)
            onAck?("K,\(type),\(id)")
        }
        slots[type] = b
    }

    func reset() { slots.removeAll() }
}
