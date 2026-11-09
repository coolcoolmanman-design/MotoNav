import Foundation
import UIKit

/// 组合动画 · 性别（男生 / 女生）
enum CenterSpriteGender: Int, CaseIterable, Identifiable {
    case male = 0, female
    var id: Int { rawValue }
    var title: String { self == .male ? "男生" : "女生" }
    var assetCode: String { self == .male ? "m" : "f" }
}

/// 组合动画 · 八车型（确认稿预制，无相册上传）
enum CenterSpriteVehicle: Int, CaseIterable, Identifiable {
    case sedan = 0, suv, supercar, motorcycle, scooter, sportbike, bicycle, foldingBike
    var id: Int { rawValue }
    var title: String {
        switch self {
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
    var assetCode: String {
        switch self {
        case .sedan: return "sedan"
        case .suv: return "suv"
        case .supercar: return "supercar"
        case .motorcycle: return "motorcycle"
        case .scooter: return "scooter"
        case .sportbike: return "sportbike"
        case .bicycle: return "bicycle"
        case .foldingBike: return "foldingBike"
        }
    }
    var isCabinCar: Bool {
        switch self {
        case .sedan, .suv, .supercar: return true
        default: return false
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
    var exhaustSharp: Bool { self == .sportbike || self == .supercar }
    var exhaustSoft: Bool { self == .scooter }
}

/// musicA.53：从确认 mockup 烤入的 Assets（`Combo_{m|f}_{veh}_{sleep|wave}`）生成两态多帧。
/// 视觉对齐舱内无头盔 / 摩托坐骑 · GTA 细像素；非程序简笔画。
enum CenterSpritePixelizer {
    static let canvas = 466
    static let sleepFrames = 6
    static let chaseFrames = 6
    static let safeY0 = 150

    struct Result {
        var sleepRGB565: [[UInt8]]
        var chaseRGB565: [[UInt8]]
        var previewSleep: [UIImage]
        var previewChase: [UIImage]
    }

    static func build(gender: CenterSpriteGender, vehicle: CenterSpriteVehicle) -> Result {
        let sleepBase = loadBase(gender: gender, vehicle: vehicle, online: false)
            ?? blankCanvas()
        let waveBase = loadBase(gender: gender, vehicle: vehicle, online: true)
            ?? sleepBase

        var sleep: [[UInt8]] = []
        var chase: [[UInt8]] = []
        var prevS: [UIImage] = []
        var prevC: [UIImage] = []

        for i in 0..<sleepFrames {
            let f = animateSleep(sleepBase, frame: i)
            sleep.append(f)
            if let img = rgb565ToUIImage(f) { prevS.append(img) }
        }
        for i in 0..<chaseFrames {
            let f = animateChase(waveBase, frame: i, vehicle: vehicle)
            chase.append(f)
            if let img = rgb565ToUIImage(f) { prevC.append(img) }
        }
        return Result(sleepRGB565: sleep, chaseRGB565: chase, previewSleep: prevS, previewChase: prevC)
    }

    // MARK: - Asset load

    private static func assetName(gender: CenterSpriteGender, vehicle: CenterSpriteVehicle, online: Bool) -> String {
        "Combo_\(gender.assetCode)_\(vehicle.assetCode)_\(online ? "wave" : "sleep")"
    }

    private static func loadBase(gender: CenterSpriteGender, vehicle: CenterSpriteVehicle, online: Bool) -> [UInt8]? {
        let name = assetName(gender: gender, vehicle: vehicle, online: online)
        guard let img = UIImage(named: name) else {
            // sleep 缺省时回退 wave（矩阵仅 wave 的车型）
            if !online, let wave = UIImage(named: assetName(gender: gender, vehicle: vehicle, online: true)) {
                return imageToRGB565(wave)
            }
            return nil
        }
        return imageToRGB565(img)
    }

    private static func imageToRGB565(_ image: UIImage) -> [UInt8]? {
        guard let cg = image.cgImage else { return nil }
        let w = canvas, h = canvas
        var rgba = [UInt8](repeating: 0, count: w * h * 4)
        guard let ctx = CGContext(data: &rgba, width: w, height: h, bitsPerComponent: 8,
                                  bytesPerRow: w * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.setFillColor(UIColor.black.cgColor)
        ctx.fill(CGRect(x: 0, y: 0, width: w, height: h))
        // 近邻缩放贴满 466（资源已是 466）
        ctx.interpolationQuality = .none
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: w, height: h))
        var out = [UInt8](repeating: 0, count: w * h * 2)
        for i in 0..<(w * h) {
            let o = i * 4
            let a = rgba[o + 3]
            let r = rgba[o], g = rgba[o + 1], b = rgba[o + 2]
            // chroma 0：近黑当透明
            if a < 20 || (r < 8 && g < 8 && b < 8) {
                out[i * 2] = 0; out[i * 2 + 1] = 0
                continue
            }
            var v = (UInt16(r & 0xF8) << 8) | (UInt16(g & 0xFC) << 3) | (UInt16(b & 0xF8) >> 3)
            if v == 0 { v = 0x0001 }
            out[i * 2] = UInt8(v & 0xFF)
            out[i * 2 + 1] = UInt8(v >> 8)
        }
        return out
    }

    private static func blankCanvas() -> [UInt8] {
        [UInt8](repeating: 0, count: canvas * canvas * 2)
    }

    // MARK: - Animations on baked stills

    private static func animateSleep(_ base: [UInt8], frame: Int) -> [UInt8] {
        var f = base
        let bob = (frame % 2 == 0) ? 0 : 1
        if bob != 0 { f = shift(f, dx: 0, dy: bob) }
        // 确认稿 sleep 图已含 Zzz；再叠相位漂浮增强多帧感
        drawZzz(&f, phase: frame)
        return f
    }

    private static func animateChase(_ base: [UInt8], frame: Int, vehicle: CenterSpriteVehicle) -> [UInt8] {
        var f = base
        if vehicle.usesExhaust {
            f = exhaustPulse(f, phase: frame, sharp: vehicle.exhaustSharp, soft: vehicle.exhaustSoft)
        } else if vehicle.usesPedal {
            f = pedalHint(f, phase: frame, small: vehicle == .foldingBike)
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
        guard x >= 0, y >= 0, x < canvas, y < canvas, rgb565 != 0 else { return }
        let i = (y * canvas + x) * 2
        f[i] = UInt8(rgb565 & 0xFF); f[i + 1] = UInt8(rgb565 >> 8)
    }

    private static func drawZzz(_ f: inout [UInt8], phase: Int) {
        let cx = canvas / 2 + 42
        let cy = safeY0 + 12 - (phase % 3) * 3
        let col: UInt16 = 0xC1BF
        drawOneZ(&f, x: cx, y: cy, s: 5, col: col)
        if phase % 2 == 0 { drawOneZ(&f, x: cx + 8, y: cy - 6, s: 4, col: col) }
        if phase % 3 == 0 { drawOneZ(&f, x: cx + 14, y: cy - 11, s: 3, col: col) }
    }

    private static func drawOneZ(_ f: inout [UInt8], x: Int, y: Int, s: Int, col: UInt16) {
        for i in 0..<s {
            putPixel(&f, x: x + i, y: y, rgb565: col)
            putPixel(&f, x: x + s - 1 - i, y: y + i, rgb565: col)
            putPixel(&f, x: x + i, y: y + s - 1, rgb565: col)
        }
    }

    private static func exhaustPulse(_ src: [UInt8], phase: Int, sharp: Bool, soft: Bool) -> [UInt8] {
        var f = src
        let baseX = canvas / 2 - 72
        let baseY = 320
        let n: Int
        if sharp { n = 3 + phase % 3 }
        else if soft { n = 1 + phase % 2 }
        else { n = 2 + phase % 2 }
        let smoke: UInt16 = 0x8410
        for i in 0..<n {
            let ox = baseX - i * 3 - (phase % 2)
            let oy = baseY - i * 2 - (phase % 3)
            for dy in 0..<(2 + i / 2) {
                for dx in 0..<(2 + i / 2) {
                    putPixel(&f, x: ox + dx, y: oy + dy, rgb565: smoke)
                }
            }
        }
        return f
    }

    private static func pedalHint(_ src: [UInt8], phase: Int, small: Bool) -> [UInt8] {
        var f = src
        let r = small ? 5 : 8
        let cy = 330
        let cxs = [canvas / 2 - 30, canvas / 2 + 28]
        let col: UInt16 = 0xC618
        let ang = Double(phase) * (.pi / 3)
        for cx in cxs {
            let x2 = cx + Int((cos(ang) * Double(r)).rounded())
            let y2 = cy + Int((sin(ang) * Double(r)).rounded())
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
