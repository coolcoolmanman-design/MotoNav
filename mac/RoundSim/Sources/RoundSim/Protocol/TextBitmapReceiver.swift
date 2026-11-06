import Foundation

/// Firmware text-bitmap `'B'` packets (4-bit gray, two pixels/byte). MVP: store dims + raw only.
final class TextBitmapReceiver {
    struct Image {
        var w = 0, h = 0
        var data = Data()
        var id = -1
        var gotParts = Set<Int>()
        var parts = 0
    }

    private(set) var images: [Int: Image] = [:]
    var onUpdate: ((Int) -> Void)?

    func handle(_ d: Data) {
        guard d.count >= 10, d[0] == UInt8(ascii: "B") else { return }
        let slot = Int(d[1]), id = Int(d[2]), part = Int(d[3]), parts = Int(d[4])
        let w = Int(d[5]) | Int(d[6]) << 8
        let h = Int(d[7])
        let off = Int(d[8]) | Int(d[9]) << 8
        guard (0...12).contains(slot) else { return }
        if w == 0 {
            images[slot] = Image(w: 0, h: 0, data: Data(), id: id, gotParts: [0], parts: 1)
            onUpdate?(slot)
            return
        }
        var img = images[slot] ?? Image()
        if img.id != id || img.w != w || img.h != h || img.parts != parts {
            img = Image(w: w, h: h, data: Data(count: max(1, (w * h + 1) / 2)), id: id, gotParts: [], parts: parts)
        }
        let payload = d.subdata(in: 10..<d.count)
        let end = min(img.data.count, off + payload.count)
        if off < end {
            img.data.replaceSubrange(off..<end, with: payload.prefix(end - off))
        }
        img.gotParts.insert(part)
        images[slot] = img
        if img.gotParts.count >= parts { onUpdate?(slot) }
    }
}
