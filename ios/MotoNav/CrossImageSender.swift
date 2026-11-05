#if canImport(UIKit)
import UIKit
import Foundation

/// musicA.26：把高德「路口放大图」转成圆屏能直接贴的 16 色索引图（bulk type 9）。
///
/// 这里做三件事，圆屏只负责展开贴图：
/// 1. **深色化** —— 高德给的是浅底位图，直接上 AMOLED 夜间就是一块白板。灰阶整体反相
///    （白→接近黑、黑→浅灰），彩色（蓝色路线、红色禁令标志、绿色路牌）保留色相只提亮，
///    这样在深底上既压住了亮度，又没有丢失有效信息。
/// 2. **缩放** —— 高德原图固定 25:16，压到 380×243（同样 25:16，不裁切不变形）。这是 466 圆屏
///    在不被圆形边缘吃掉圆角的前提下能放的最大尺寸（y=105 那行圆的半宽只有 195px）。
/// 3. **编码** —— 固定 16 色调色板量化 + nibble RLE。实测压缩后 12~20KB，BLE 一秒内推完；
///    直接传 RGB565 要 180KB，路口前 300 米才开始要图，根本来不及。
enum CrossImageSender {
    static let W = 380
    static let H = 243
    static let bulkType = 9

    /// 10 级灰阶 + 6 个语义色。灰阶够细才不会出现色带，彩色只留路口图真正用得到的几种。
    private static let hexPalette: [UInt32] = [
        0x0E1116, 0x1A1F27, 0x262C36, 0x333A46, 0x454E5C,
        0x5E6878, 0x7C8798, 0x9AA5B5, 0xC2CBD8, 0xF0F3F8,
        0x2F6FE0, 0x4F9BFF, 0xE24B4A, 0xEF9F27, 0x4CAF7D, 0x8A5CF0,
    ]
    private static let rgb565: [UInt16] = hexPalette.map { h in
        let r = UInt16((h >> 16) & 0xFF), g = UInt16((h >> 8) & 0xFF), b = UInt16(h & 0xFF)
        return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)
    }
    private static let rgb888: [(Int, Int, Int)] = hexPalette.map { h in
        (Int((h >> 16) & 0xFF), Int((h >> 8) & 0xFF), Int(h & 0xFF))
    }

    private static var lastHash: UInt64 = 0
    private static var lastSize = 0
    private static var lastId = 0
    private static var sent = 0
    private static let queue = DispatchQueue(label: "motonav.cross", qos: .utility)

    /// 返回一句给调试卡看的结论；真正的编码在后台队列做，完成后再回主线程发。
    @discardableResult
    static func send(_ image: UIImage) -> String {
        guard Settings.shared.crossOn else { return "路口放大图已关闭" }
        guard BleLink.shared.crossSupported else { return "固件不支持路口图（< musicA.26）" }
        queue.async {
            guard let idx = quantize(image) else { return }
            let packet = encode(idx)
            let hash = fnv(idx)
            DispatchQueue.main.async {
                if hash == lastHash && packet.count == lastSize { return }   // 同一个路口重复推送，跳过
                lastHash = hash; lastSize = packet.count
                lastId = (lastId + 1) & 0xFF
                sent += 1
                BleLink.shared.sendBulk(type: bulkType, id: lastId, data: packet)
                print("[Cross] 已下发 #\(sent) \(packet.count / 1024)KB id=\(lastId)")
            }
        }
        return "路口图处理中"
    }

    /// 高德隐藏路口图（通过路口）→ 让圆屏退回小地图
    static func hide() {
        BleLink.shared.send("#CXH")
    }

    // MARK: - 像素处理

    private static func quantize(_ image: UIImage) -> [UInt8]? {
        guard let cg = image.cgImage else { return nil }
        var rgba = [UInt8](repeating: 0, count: W * H * 4)
        guard let ctx = CGContext(data: &rgba, width: W, height: H, bitsPerComponent: 8,
                                  bytesPerRow: W * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.interpolationQuality = .high
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: W, height: H))

        var out = [UInt8](repeating: 0, count: W * H)
        for i in 0..<(W * H) {
            let r = Int(rgba[i * 4]), g = Int(rgba[i * 4 + 1])
            let b = Int(rgba[i * 4 + 2]), a = Int(rgba[i * 4 + 3])
            if a < 110 { out[i] = 0; continue }                       // 透明区域按背景处理
            let mx = max(r, max(g, b)), mn = min(r, min(g, b))
            let chroma = mx - mn
            let lum = 0.299 * Double(r) + 0.587 * Double(g) + 0.114 * Double(b)
            // 判定“灰”必须用绝对色差：深色文字的 R/G/B 只差十几个数，相对饱和度却有 0.18，
            // 用 rel-sat 会把黑字误判成彩色 → 提亮后只剩一团灰蓝，字反而看不清。
            let isGray = chroma < 26 || (mx > 0 && Double(chroma) / Double(mx) < 0.14)
            var rr = r, gg = g, bb = b
            if isGray {
                // 灰阶：反相。白(255)→20，黑(0)→160，中灰→96
                let v = Int(20.0 + (255.0 - lum) * 0.55)
                rr = v; gg = v; bb = v
            } else {
                // 彩色：保留色相，把明度抬到深底上也看得清
                let target = 96.0 + lum * 0.50
                let k = lum > 1 ? target / lum : 1.0
                rr = min(255, Int(Double(r) * k))
                gg = min(255, Int(Double(g) * k))
                bb = min(255, Int(Double(b) * k))
            }
            out[i] = nearest(rr, gg, bb)
        }
        return out
    }

    private static func nearest(_ r: Int, _ g: Int, _ b: Int) -> UInt8 {
        var best = 0, bestD = Int.max
        for i in 0..<rgb888.count {
            let c = rgb888[i]
            let dr = r - c.0, dg = g - c.1, db = b - c.2
            let d = dr * dr * 2 + dg * dg * 4 + db * db          // 近似人眼权重，绿>红>蓝
            if d < bestD { bestD = d; best = i }
        }
        return UInt8(best)
    }

    // MARK: - 打包

    /// 'X' | ver=1 | u16 w | u16 h | 16×u16 RGB565 | RLE（高 4 位 n：n<15 → n+1 个；n=15 → 16+下一字节 个）
    private static func encode(_ idx: [UInt8]) -> Data {
        var b = [UInt8]()
        b.reserveCapacity(idx.count / 4 + 64)
        b.append(0x58); b.append(1)
        b.append(UInt8(W & 0xFF)); b.append(UInt8(W >> 8))
        b.append(UInt8(H & 0xFF)); b.append(UInt8(H >> 8))
        for c in rgb565 { b.append(UInt8(c & 0xFF)); b.append(UInt8(c >> 8)) }
        var i = 0
        let n = idx.count
        while i < n {
            let c = idx[i] & 15
            var run = 1
            while i + run < n && run < 271 && (idx[i + run] & 15) == c { run += 1 }
            if run <= 15 { b.append(UInt8((run - 1) << 4) | c) }
            else { b.append(0xF0 | c); b.append(UInt8(run - 16)) }
            i += run
        }
        return Data(b)
    }

    private static func fnv(_ idx: [UInt8]) -> UInt64 {
        var h: UInt64 = 0xcbf29ce484222325
        for v in idx { h = (h ^ UInt64(v)) &* 0x100000001b3 }
        return h
    }
}
#endif
