import Foundation

/// 高德原生 3D 底图（P3D，BLE bulk type 13）打包。
///
/// 协议见 `firmware/docs/persp-3d-protocol.md`：
///   头 32B（'P3D1' + version + flags + frame + w/h + 锚点世界坐标 + 朝向 + 俯仰 + dMin/dMax + lutN）
///   32B 调色板（16 色 RGB565，flags.bit0 = 1）
///   lutN × {u16 画布行 v, u16 该距离处每画布列多少毫米}
///   4bit 索引像素，行优先（高半字节 = 偶数列），(run-1, color) 双字节游程
///
/// 没有 UIKit/CoreGraphics 依赖，方便用 `swiftc` 单独跑测试（ios/tests/persp3d）。
enum Persp3DPacket {
    static let version: UInt8 = 1
    static let maxSide = 1024
    static let paletteSize = 16

    struct LUTPoint {
        let v: Double          // 该距离对应的画布行（像素）
        let mm: Double         // 该距离处每个画布列多少毫米
    }

    /// 把一帧透视图打包成固件能解析的 P3D。`pixels` 是 ARGB（R = c>>16，G = c>>8，B = c&255）。
    /// 屏幕直出（flags.bit1）：`screenScale` 是固件导航时冻结的缩放（px/m，写进 reserved×10000），
    /// `fog` 是「高德没覆盖」的哨兵颜色，编码成索引 15，固件用地面底色画。
    static func encode(frame: Int, width w: Int, height h: Int,
                       anchorX: Int32, anchorY: Int32,
                       bearing: Double, pitch: Double,
                       dMin: Int, dMax: Int,
                       lut: [LUTPoint], pixels: [UInt32],
                       screenScale: Double? = nil, fog: UInt32? = nil) -> Data? {
        guard w >= 8, h >= 8, w <= maxSide, h <= maxSide else { return nil }
        guard pixels.count >= w * h else { return nil }
        guard lut.count >= 2, lut.count <= 64 else { return nil }
        guard dMin >= 1, dMax > dMin * 2, dMax <= 65535 else { return nil }

        let (palette, indices) = quantize(pixels, count: w * h, fog: fog)

        var out = Data(capacity: 64 + lut.count * 4 + w * h / 3)
        out.append(contentsOf: [0x50, 0x33, 0x44, 0x31])          // 'P','3','D','1'
        out.append(version)
        out.append(screenScale == nil ? 1 : 3)                    // flags: bit0 带调色板，bit1 屏幕直出
        appendU16(&out, UInt16(clamping: frame))
        appendU16(&out, UInt16(clamping: w))
        appendU16(&out, UInt16(clamping: h))
        appendU32(&out, UInt32(bitPattern: anchorX))
        appendU32(&out, UInt32(bitPattern: anchorY))
        appendU16(&out, UInt16(bitPattern: Int16(clamping: Int((bearing * 10).rounded()))))
        appendU16(&out, UInt16(bitPattern: Int16(clamping: Int((pitch * 10).rounded()))))
        appendU16(&out, UInt16(clamping: dMin))
        appendU16(&out, UInt16(clamping: dMax))
        appendU16(&out, UInt16(clamping: lut.count))
        appendU16(&out, UInt16(clamping: Int(((screenScale ?? 0) * 10000).rounded())))   // reserved：屏幕直出时为缩放×10000
        for c in palette { appendU16(&out, c) }
        for p in lut {
            appendU16(&out, UInt16(clamping: Int(p.v.rounded())))
            appendU16(&out, UInt16(clamping: Int(p.mm.rounded())))
        }

        let total = w * h
        var i = 0
        while i < total {
            let c = indices[i]
            var run = 1
            while i + run < total, indices[i + run] == c, run < 256 { run += 1 }
            out.append(UInt8(run - 1))
            out.append(c)
            i += run
        }
        return out
    }

    // MARK: - 调色板

    /// RGB555 频次取前 15 色，第 16 格固定纯黑兜底。返回 (RGB565 调色板, 每像素 4bit 索引)。
    static func quantize(_ pixels: [UInt32], count: Int, fog: UInt32? = nil) -> ([UInt16], [UInt8]) {
        // 有 fog 哨兵时：索引 15 固定给 fog，其余 14 格给颜色 + 1 格黑。
        let colorSlots = fog == nil ? paletteSize - 1 : paletteSize - 2
        var hist = [Int: Int](minimumCapacity: 512)
        for i in 0..<count {
            let c = pixels[i]
            if let f = fog, c == f { continue }
            let r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255
            let key = Int((r >> 3) << 10 | (g >> 3) << 5 | (b >> 3))
            hist[key, default: 0] += 1
        }
        let keys = hist.sorted { $0.value > $1.value }.prefix(colorSlots).map { $0.key }
        var palette = keys.map(rgb565)
        while palette.count < colorSlots { palette.append(0) }
        palette.append(0)                                          // 黑
        if let f = fog {
            let r = Int((f >> 16) & 255), g = Int((f >> 8) & 255), b = Int(f & 255)
            palette.append(rgb565(Int((r >> 3) << 10 | (g >> 3) << 5 | (b >> 3))))   // 索引 15：fog 底色
        } else {
            palette.append(0)                                      // 索引 15：纯黑
        }

        var indexOf = [Int: UInt8](minimumCapacity: 512)
        let nearest = keys + [0]
        var indices = [UInt8](repeating: UInt8(paletteSize - 1), count: count)
        for i in 0..<count {
            let c = pixels[i]
            if let f = fog, c == f { indices[i] = UInt8(paletteSize - 1); continue }
            let r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255
            let key = Int((r >> 3) << 10 | (g >> 3) << 5 | (b >> 3))
            if let hit = indexOf[key] { indices[i] = hit; continue }
            var best = 0, bestD = Int.max
            let kr = key >> 10, kg = (key >> 5) & 31, kb = key & 31
            for (n, candidate) in nearest.enumerated() {
                let dr = kr - (candidate >> 10)
                let dg = kg - ((candidate >> 5) & 31)
                let db = kb - (candidate & 31)
                let d = dr * dr + dg * dg + db * db
                if d < bestD { bestD = d; best = n }
            }
            let value = UInt8(best)
            indexOf[key] = value
            indices[i] = value
        }
        return (palette, indices)
    }

    /// RGB555 → RGB565。绿通道有 6 位，把 5 位补成 6 位（低位复制高位），否则整体会偏暗一档。
    private static func rgb565(_ key555: Int) -> UInt16 {
        let r = (key555 >> 10) & 31, g = (key555 >> 5) & 31, b = key555 & 31
        let g6 = (g << 1) | (g >> 4)
        return UInt16((r << 11) | (g6 << 5) | b)
    }

    private static func appendU16(_ out: inout Data, _ v: UInt16) {
        out.append(UInt8(v & 0xFF))
        out.append(UInt8((v >> 8) & 0xFF))
    }

    private static func appendU32(_ out: inout Data, _ v: UInt32) {
        out.append(UInt8(v & 0xFF))
        out.append(UInt8((v >> 8) & 0xFF))
        out.append(UInt8((v >> 16) & 0xFF))
        out.append(UInt8((v >> 24) & 0xFF))
    }
}
