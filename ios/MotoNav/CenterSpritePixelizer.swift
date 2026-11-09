import Foundation
import UIKit
import Vision

/// 中央槽自定义：Vision 抠人 / 矩形兜底 → 厂编密度像素化 → 两态多帧（sleep Zzz / chase 挥手·尾气·踏频）。
enum CenterSpriteSubject: Int, CaseIterable, Identifiable {
    case person = 0
    case sedan, suv, supercar, motorcycle, scooter, sportbike, bicycle, foldingBike
    var id: Int { rawValue }
    var title: String {
        switch self {
        case .person: return "人物"
        case .sedan: return "轿车"
        case .suv: return "SUV"
        case .supercar: return "超跑"
        case .motorcycle: return "摩托车"
        case .scooter: return "踏板摩托"
        case .sportbike: return "竞速摩托"
        case .bicycle: return "自行车"
        case .foldingBike: return "折叠小轮"
        }
    }
    var usesExhaust: Bool {
        switch self {
        case .sedan, .suv, .supercar, .motorcycle, .scooter, .sportbike: return true
        default: return false
        }
    }
    var usesPedal: Bool {
        switch self {
        case .bicycle, .foldingBike: return true
        default: return false
        }
    }
}

enum CenterSpritePixelizer {
    static let canvas = 466
    /// 对齐厂编 GTA ≈163px 宽：逻辑格 × cell3
    static let cell = 3
    static let contentMaxW = 162
    static let safeY0 = 150
    static let safeY1 = 348
    static let sleepFrames = 6
    static let chaseFrames = 6

    struct Result {
        var sleepRGB565: [[UInt8]]
        var chaseRGB565: [[UInt8]]
        var previewSleep: [UIImage]
        var previewChase: [UIImage]
    }

    /// 选图 → 抠人/矩形 → 像素化 → 两态帧。`manualRect` 为原图归一化裁切（车辆 / 失败兜底）。
    static func build(from image: UIImage, subject: CenterSpriteSubject, manualRect: CGRect?) -> Result? {
        guard let rgbaCut = cutoutRGBA(image, subject: subject, manualRect: manualRect) else { return nil }
        guard let base = pixelizeToCanvas(rgbaCut) else { return nil }
        var sleep: [[UInt8]] = []
        var chase: [[UInt8]] = []
        var prevS: [UIImage] = []
        var prevC: [UIImage] = []
        for i in 0..<sleepFrames {
            let f = animateSleep(base, frame: i, subject: subject)
            sleep.append(f)
            if let img = rgb565ToUIImage(f) { prevS.append(img) }
        }
        for i in 0..<chaseFrames {
            let f = animateChase(base, frame: i, subject: subject)
            chase.append(f)
            if let img = rgb565ToUIImage(f) { prevC.append(img) }
        }
        return Result(sleepRGB565: sleep, chaseRGB565: chase, previewSleep: prevS, previewChase: prevC)
    }

    // MARK: - Cutout

    private static func cutoutRGBA(_ image: UIImage, subject: CenterSpriteSubject, manualRect: CGRect?) -> [UInt8]? {
        if subject != .person {
            return rectangularRGBA(image, normalized: manualRect ?? CGRect(x: 0.15, y: 0.15, width: 0.7, height: 0.7))
        }
        if let person = personSegmentRGBA(image) { return person }
        return rectangularRGBA(image, normalized: manualRect ?? CGRect(x: 0.2, y: 0.1, width: 0.6, height: 0.8))
    }

    private static func personSegmentRGBA(_ image: UIImage) -> [UInt8]? {
        guard let cg = image.cgImage else { return nil }
        let req = VNGeneratePersonSegmentationRequest()
        req.qualityLevel = .balanced
        req.outputPixelFormat = kCVPixelFormatType_OneComponent8
        let handler = VNImageRequestHandler(cgImage: cg, options: [:])
        do { try handler.perform([req]) } catch { return nil }
        guard let mask = req.results?.first?.pixelBuffer else { return nil }
        return compositePerson(cg, mask: mask)
    }

    private static func compositePerson(_ cg: CGImage, mask: CVPixelBuffer) -> [UInt8]? {
        let w = cg.width, h = cg.height
        var rgba = [UInt8](repeating: 0, count: w * h * 4)
        guard let ctx = CGContext(data: &rgba, width: w, height: h, bitsPerComponent: 8,
                                  bytesPerRow: w * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: w, height: h))
        CVPixelBufferLockBaseAddress(mask, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(mask, .readOnly) }
        guard let base = CVPixelBufferGetBaseAddress(mask) else { return nil }
        let mw = CVPixelBufferGetWidth(mask), mh = CVPixelBufferGetHeight(mask)
        let mrow = CVPixelBufferGetBytesPerRow(mask)
        let mptr = base.assumingMemoryBound(to: UInt8.self)
        for y in 0..<h {
            let my = min(mh - 1, y * mh / max(h, 1))
            for x in 0..<w {
                let mx = min(mw - 1, x * mw / max(w, 1))
                let a = mptr[my * mrow + mx]
                let i = (y * w + x) * 4
                if a < 40 {
                    rgba[i] = 0; rgba[i + 1] = 0; rgba[i + 2] = 0; rgba[i + 3] = 0
                } else {
                    rgba[i + 3] = a
                }
            }
        }
        // 打包成「带 alpha 的原尺寸」；后续 pixelize 统一缩放
        return packSized(rgba, w: w, h: h)
    }

    private static func rectangularRGBA(_ image: UIImage, normalized: CGRect) -> [UInt8]? {
        guard let cg = image.cgImage else { return nil }
        let nw = CGFloat(cg.width), nh = CGFloat(cg.height)
        let r = CGRect(x: normalized.minX * nw, y: normalized.minY * nh,
                       width: normalized.width * nw, height: normalized.height * nh)
            .integral.intersection(CGRect(x: 0, y: 0, width: nw, height: nh))
        guard r.width > 8, r.height > 8, let cut = cg.cropping(to: r) else { return nil }
        let w = cut.width, h = cut.height
        var rgba = [UInt8](repeating: 0, count: w * h * 4)
        guard let ctx = CGContext(data: &rgba, width: w, height: h, bitsPerComponent: 8,
                                  bytesPerRow: w * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.draw(cut, in: CGRect(x: 0, y: 0, width: w, height: h))
        // 近黑边缘外视为透明（矩形兜底不做软抠）
        for i in 0..<(w * h) {
            let o = i * 4
            if rgba[o] < 8 && rgba[o + 1] < 8 && rgba[o + 2] < 8 { rgba[o + 3] = 0 }
            else { rgba[o + 3] = 255 }
        }
        return packSized(rgba, w: w, h: h)
    }

    /// 前 8 字节存 w/h le16×2，后跟 rgba
    private static func packSized(_ rgba: [UInt8], w: Int, h: Int) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: 8 + rgba.count)
        out[0] = UInt8(w & 0xFF); out[1] = UInt8((w >> 8) & 0xFF)
        out[2] = UInt8(h & 0xFF); out[3] = UInt8((h >> 8) & 0xFF)
        for i in 0..<rgba.count { out[8 + i] = rgba[i] }
        return out
    }

    private static func unpackSized(_ packed: [UInt8]) -> (w: Int, h: Int, rgba: [UInt8])? {
        guard packed.count >= 8 else { return nil }
        let w = Int(packed[0]) | (Int(packed[1]) << 8)
        let h = Int(packed[2]) | (Int(packed[3]) << 8)
        guard w > 0, h > 0, packed.count >= 8 + w * h * 4 else { return nil }
        return (w, h, Array(packed[8..<]))
    }

    // MARK: - Pixelize into 466 canvas (A.49 safe box)

    private static func pixelizeToCanvas(_ packed: [UInt8]) -> [UInt8]? {
        guard let (sw, sh, rgba) = unpackSized(packed) else { return nil }
        let maxW = contentMaxW / cell
        let maxH = (safeY1 - safeY0) / cell
        let scale = min(CGFloat(maxW) / CGFloat(sw), CGFloat(maxH) / CGFloat(sh), 1)
        let gw = max(8, Int((CGFloat(sw) * scale).rounded(.down)))
        let gh = max(8, Int((CGFloat(sh) * scale).rounded(.down)))
        // 最近邻降采样 + 原图量化（每通道 5bit 近似）
        var grid = [UInt16](repeating: 0, count: gw * gh) // 0 = 透明
        for gy in 0..<gh {
            let sy = min(sh - 1, gy * sh / gh)
            for gx in 0..<gw {
                let sx = min(sw - 1, gx * sw / gw)
                let o = (sy * sw + sx) * 4
                if rgba[o + 3] < 40 { continue }
                var r = rgba[o], g = rgba[o + 1], b = rgba[o + 2]
                r = (r & 0xF8); g = (g & 0xFC); b = (b & 0xF8)
                var v = (UInt16(r) << 8) | (UInt16(g) << 3) | (UInt16(b) >> 3)
                if v == 0 { v = 0x0001 } // 避免真黑镂空
                grid[gy * gw + gx] = v
            }
        }
        // 硬描边
        var edged = grid
        for y in 0..<gh {
            for x in 0..<gw {
                if grid[y * gw + x] == 0 { continue }
                for oy in -1...1 {
                    for ox in -1...1 {
                        if ox == 0 && oy == 0 { continue }
                        let nx = x + ox, ny = y + oy
                        if nx < 0 || ny < 0 || nx >= gw || ny >= gh || grid[ny * gw + nx] == 0 {
                            edged[y * gw + x] = 0x0841 // 近黑描边（非 0）
                            break
                        }
                    }
                }
            }
        }
        // 放大到 cell，居中贴进安全盒
        let pw = gw * cell, ph = gh * cell
        let ox = (canvas - pw) / 2
        let oy = safeY0 + max(0, ((safeY1 - safeY0) - ph) / 2)
        var out = [UInt8](repeating: 0, count: canvas * canvas * 2)
        for y in 0..<ph {
            let gy = y / cell
            for x in 0..<pw {
                let gx = x / cell
                let c = edged[gy * gw + gx]
                if c == 0 { continue }
                let dx = ox + x, dy = oy + y
                if dx < 0 || dy < 0 || dx >= canvas || dy >= canvas { continue }
                let i = (dy * canvas + dx) * 2
                out[i] = UInt8(c & 0xFF)
                out[i + 1] = UInt8(c >> 8)
            }
        }
        return out
    }

    // MARK: - Animations

    private static func animateSleep(_ base: [UInt8], frame: Int, subject: CenterSpriteSubject) -> [UInt8] {
        var f = base
        // 轻微上下呼吸 + Zzz
        let bob = (frame % 2 == 0) ? 0 : 1
        if bob != 0 { f = shift(f, dx: 0, dy: bob) }
        if subject == .person || true {
            drawZzz(&f, phase: frame)
        }
        return f
    }

    private static func animateChase(_ base: [UInt8], frame: Int, subject: CenterSpriteSubject) -> [UInt8] {
        var f = base
        switch subject {
        case .person:
            // 挥手：右侧上角小块位移
            f = waveHand(f, phase: frame)
        case .bicycle, .foldingBike:
            f = pedalWheels(f, phase: frame, small: subject == .foldingBike)
        default:
            f = exhaustPulse(f, phase: frame, sharp: subject == .sportbike || subject == .supercar)
        }
        return f
    }

    private static func shift(_ src: [UInt8], dx: Int, dy: Int) -> [UInt8] {
        var dst = [UInt8](repeating: 0, count: src.count)
        for y in 0..<canvas {
            for x in 0..<canvas {
                let sx = x - dx, sy = y - dy
                if sx < 0 || sy < 0 || sx >= canvas || sy >= canvas { continue }
                let si = (sy * canvas + sx) * 2
                let di = (y * canvas + x) * 2
                dst[di] = src[si]; dst[di + 1] = src[si + 1]
            }
        }
        return dst
    }

    private static func putPixel(_ f: inout [UInt8], x: Int, y: Int, rgb565: UInt16) {
        guard x >= 0, y >= 0, x < canvas, y < canvas else { return }
        if rgb565 == 0 { return }
        let i = (y * canvas + x) * 2
        f[i] = UInt8(rgb565 & 0xFF); f[i + 1] = UInt8(rgb565 >> 8)
    }

    private static func drawZzz(_ f: inout [UInt8], phase: Int) {
        let cx = canvas / 2 + 40
        let cy = safeY0 + 18 - (phase % 3) * 4
        let col: UInt16 = 0xFFFF
        // 简易 Z
        for i in 0..<6 {
            putPixel(&f, x: cx + i, y: cy, rgb565: col)
            putPixel(&f, x: cx + 5 - i, y: cy + i, rgb565: col)
            putPixel(&f, x: cx + i, y: cy + 5, rgb565: col)
        }
        if phase % 2 == 0 {
            for i in 0..<4 {
                putPixel(&f, x: cx + 10 + i, y: cy - 8, rgb565: col)
                putPixel(&f, x: cx + 13 - i, y: cy - 8 + i, rgb565: col)
                putPixel(&f, x: cx + 10 + i, y: cy - 4, rgb565: col)
            }
        }
    }

    private static func waveHand(_ src: [UInt8], phase: Int) -> [UInt8] {
        var f = src
        let lift = (phase % 3) * 2
        let hx = canvas / 2 + 28, hy = safeY0 + 70
        for y in 0..<14 {
            for x in 0..<10 {
                let si = ((hy + y) * canvas + (hx + x)) * 2
                guard si + 1 < src.count else { continue }
                let c = UInt16(src[si]) | (UInt16(src[si + 1]) << 8)
                if c == 0 { continue }
                putPixel(&f, x: hx + x, y: hy + y - lift, rgb565: c)
                putPixel(&f, x: hx + x, y: hy + y, rgb565: 0x0001) // 擦原位近黑占位后清：用透明重画底 — 简化为覆盖
                // 实际：原位置 0
                let oi = ((hy + y) * canvas + (hx + x)) * 2
                f[oi] = 0; f[oi + 1] = 0
                putPixel(&f, x: hx + x, y: hy + y - lift, rgb565: c == 0 ? 0x0001 : c)
            }
        }
        return f
    }

    private static func exhaustPulse(_ src: [UInt8], phase: Int, sharp: Bool) -> [UInt8] {
        var f = src
        let baseX = canvas / 2 - 50
        let baseY = safeY1 - 40
        let n = sharp ? (3 + phase % 3) : (2 + phase % 2)
        let smoke: UInt16 = 0x8410
        for i in 0..<n {
            let ox = baseX - i * 4 - (phase % 2)
            let oy = baseY - i * 3 - (phase % 3)
            for dy in 0..<(2 + i) {
                for dx in 0..<(2 + i) {
                    putPixel(&f, x: ox + dx, y: oy + dy, rgb565: smoke)
                }
            }
        }
        return f
    }

    private static func pedalWheels(_ src: [UInt8], phase: Int, small: Bool) -> [UInt8] {
        var f = src
        let r = small ? 6 : 10
        let cy = safeY1 - 36
        let cxs = [canvas / 2 - 28, canvas / 2 + 28]
        let col: UInt16 = 0xC618
        for cx in cxs {
            let ang = Double(phase) * (.pi / 3)
            let x2 = cx + Int((cos(ang) * Double(r)).rounded())
            let y2 = cy + Int((sin(ang) * Double(r)).rounded())
            // 辐条两点
            putPixel(&f, x: cx, y: cy, rgb565: col)
            putPixel(&f, x: x2, y: y2, rgb565: col)
            putPixel(&f, x: cx - (x2 - cx), y: cy - (y2 - cy), rgb565: col)
        }
        return f
    }

    private static func rgb565ToUIImage(_ raw: [UInt8]) -> UIImage? {
        var rgba = [UInt8](repeating: 0, count: canvas * canvas * 4)
        for i in 0..<(canvas * canvas) {
            let c = UInt16(raw[i * 2]) | (UInt16(raw[i * 2 + 1]) << 8)
            let o = i * 4
            if c == 0 {
                rgba[o + 3] = 0
            } else {
                rgba[o] = UInt8(((c >> 11) & 0x1F) << 3)
                rgba[o + 1] = UInt8(((c >> 5) & 0x3F) << 2)
                rgba[o + 2] = UInt8((c & 0x1F) << 3)
                rgba[o + 3] = 255
            }
        }
        let fmt = UIGraphicsImageRendererFormat()
        fmt.scale = 1
        fmt.opaque = false
        let r = UIGraphicsImageRenderer(size: CGSize(width: canvas, height: canvas), format: fmt)
        return r.image { ctx in
            guard let cg = CGContext(data: &rgba, width: canvas, height: canvas, bitsPerComponent: 8,
                                     bytesPerRow: canvas * 4, space: CGColorSpaceCreateDeviceRGB(),
                                     bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue),
                  let img = cg.makeImage() else { return }
            ctx.cgContext.draw(img, in: CGRect(x: 0, y: 0, width: canvas, height: canvas))
        }
    }
}
