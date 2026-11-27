import Foundation

func check(_ ok: Bool, _ what: String) {
    if !ok { print("FAIL \(what)"); fatalError(what) }
}

// 32×16 合成图：顶部一行纯黑、中间一块白、其余深蓝灰
let w = 32, h = 16
var px = [UInt32](repeating: 0xFF30506E, count: w * h)
for x in 0..<w { px[x] = 0xFF000000 }
for y in 6..<12 {
    for x in 10..<22 { px[y * w + x] = 0xFFF2F2F2 }
}
let lut = (0..<8).map { i in Persp3DPacket.LUTPoint(v: Double(h - 1 - i * 2), mm: 900.0 + Double(i) * 50.0) }
guard let data = Persp3DPacket.encode(frame: 9, width: w, height: h, anchorX: -1234, anchorY: 5678,
                                      bearing: -91.5, pitch: 46, dMin: 6, dMax: 260,
                                      lut: lut, pixels: px) else { fatalError("encode nil") }
let b = [UInt8](data)

func u16(_ o: Int) -> Int { Int(b[o]) | Int(b[o + 1]) << 8 }
func i32(_ o: Int) -> Int32 {
    let v = UInt32(b[o]) | UInt32(b[o + 1]) << 8 | UInt32(b[o + 2]) << 16 | UInt32(b[o + 3]) << 24
    return Int32(bitPattern: v)
}

check(b.count > 100, "size")
check(b[0] == 0x50 && b[1] == 0x33 && b[2] == 0x44 && b[3] == 0x31, "magic P3D1")
check(b[4] == 1, "version")
check(b[5] & 1 == 1, "palette flag")
check(u16(6) == 9, "frame")
check(u16(8) == w && u16(10) == h, "w/h")
check(i32(12) == -1234 && i32(16) == 5678, "anchor")
check(Int(Int16(bitPattern: UInt16(u16(20)))) == -915, "bearing x10")
check(Int(Int16(bitPattern: UInt16(u16(22)))) == 460, "pitch x10")
check(u16(24) == 6 && u16(26) == 260, "distance range")
check(u16(28) == 8, "lutN")

var pos = 32
var pal = [Int]()
for k in 0..<16 { pal.append(u16(pos + 2 * k)) }
pos += 32
var vs = [Int](), mms = [Int]()
for k in 0..<8 {
    vs.append(u16(pos + 4 * k))
    mms.append(u16(pos + 4 * k + 2))
}
pos += 32
check(vs == [15, 13, 11, 9, 7, 5, 3, 1], "lut v sequence")
check(mms[0] == 900 && mms[7] == 1250, "lut mm")

// 按固件的解码逻辑把游程展开，逐像素对比
var idx = [UInt8](repeating: 255, count: w * h)
var i = 0
while pos + 1 < b.count && i < w * h {
    let run = Int(b[pos]) + 1, c = b[pos + 1] & 15
    pos += 2
    for _ in 0..<run {
        if i < w * h { idx[i] = c }
        i += 1
    }
}
check(i == w * h, "rle covers every pixel")
check(idx[0] == idx[1], "black row flat")
check(idx[0] != idx[6 * w + 12], "black differs from white block")
check(idx[6 * w + 12] == idx[10 * w + 18], "white block flat")
check(Set(pal).count >= 3, "palette has content")
check(pal.count == 16, "palette size")
print("persp3d OK bytes=\(b.count) colors=\(Set(pal).count)")
