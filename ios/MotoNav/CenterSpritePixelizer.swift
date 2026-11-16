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

/// musicA.53 / iOS 0.16.2.51：从确认 mockup 烤入的 Assets 生成两态多帧。
/// 0.16.2.51：修 CG 行序倒置；content bbox 放大贴满安全盒（预览/CSP 同帧正立可读）。
enum CenterSpritePixelizer {
    static let canvas = 466
    static let sleepFrames = 6
    static let chaseFrames = 6
    /// 中央槽安全盒（对齐 GTA）；主体放大填满此区，避免预览细条
    static let safeY0 = 140
    static let safeY1 = 360
    static let targetMaxW = 320

    struct Result {
        var sleepRGB565: [[UInt8]]
        var chaseRGB565: [[UInt8]]
        var previewSleep: [UIImage]
        var previewChase: [UIImage]
    }

    /// fix4：两端（App 预览 / 圆屏）观察到同一份 RGB565 均上下倒置，且解释方向一致，
    /// 因此在唯一的生成出口对每帧做一次垂直行反转 —— 预览与推送共用翻转后的帧，同时变正。
    private static func flipY(_ src: [UInt8]) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: src.count)
        for y in 0..<canvas {
            let sy = canvas - 1 - y
            for x in 0..<canvas {
                let si = (sy * canvas + x) * 2, di = (y * canvas + x) * 2
                out[di] = src[si]; out[di + 1] = src[si + 1]
            }
        }
        return out
    }

    /// 8/16 组合的 sleep 与 wave 烤帧字节相同：给未连接态整体压暗，让两态明显区分。
    private static func dimBase(_ src: [UInt8]) -> [UInt8] {
        var out = src
        for i in 0..<(canvas * canvas) {
            let c = UInt16(src[i*2]) | (UInt16(src[i*2+1]) << 8)
            if c == 0 { continue }
            let r = UInt16(((c >> 11) & 0x1F) * 72 / 100)
            let g = UInt16(((c >> 5) & 0x3F) * 72 / 100)
            let b = UInt16((c & 0x1F) * 72 / 100)
            let v = (r << 11) | (g << 5) | b
            let vv: UInt16 = v == 0 ? 0x0001 : v
            out[i*2] = UInt8(vv & 0xFF); out[i*2+1] = UInt8(vv >> 8)
        }
        return out
    }

    static func build(gender: CenterSpriteGender, vehicle: CenterSpriteVehicle) -> Result {
        var sleepBase = loadBase(gender: gender, vehicle: vehicle, online: false)
            ?? blankCanvas()
        let waveBase = loadBase(gender: gender, vehicle: vehicle, online: true)
            ?? sleepBase
        if sleepBase == waveBase { sleepBase = dimBase(sleepBase) }

        var sleep: [[UInt8]] = []
        var chase: [[UInt8]] = []
        var prevS: [UIImage] = []
        var prevC: [UIImage] = []

        for i in 0..<sleepFrames {
            let f = flipY(animateSleep(sleepBase, frame: i))
            sleep.append(f)
            if let img = rgb565ToUIImage(f) { prevS.append(img) }
        }
        for i in 0..<chaseFrames {
            let f = flipY(animateChase(waveBase, frame: i, vehicle: vehicle))
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
            if !online, let wave = UIImage(named: assetName(gender: gender, vehicle: vehicle, online: true)) {
                return normalizeAsset(wave)
            }
            return nil
        }
        return normalizeAsset(img)
    }

    /// 烤帧 PNG 本身正立；CGContext 默认底左原点会倒置 —— 翻 Y 后按 content bbox 放大贴入安全盒。
    private static func normalizeAsset(_ image: UIImage) -> [UInt8]? {
        guard let rgbaFull = decodeTopLeftRGBA(image) else { return nil }
        guard let fitted = refitContentToSafeBox(rgbaFull) else { return nil }
        return rgbaToRGB565(fitted)
    }

    /// UIKit 顶左 → RGBA buffer（y=0 为顶），避免预览/CSP 倒立。
    private static func decodeTopLeftRGBA(_ image: UIImage) -> [UInt8]? {
        guard let cg = image.cgImage else { return nil }
        let w = cg.width, h = cg.height
        var rgba = [UInt8](repeating: 0, count: w * h * 4)
        guard let ctx = CGContext(data: &rgba, width: w, height: h, bitsPerComponent: 8,
                                  bytesPerRow: w * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        // CG 底左 → 翻成顶左行序（与固件 framebuffer / 既往 CSP 一致）
        ctx.translateBy(x: 0, y: CGFloat(h))
        ctx.scaleBy(x: 1, y: -1)
        ctx.interpolationQuality = .none
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: w, height: h))
        // 前 8 字节 w/h le16
        var pack = [UInt8](repeating: 0, count: 8 + rgba.count)
        pack[0] = UInt8(w & 0xFF); pack[1] = UInt8((w >> 8) & 0xFF)
        pack[2] = UInt8(h & 0xFF); pack[3] = UInt8((h >> 8) & 0xFF)
        for i in 0..<rgba.count { pack[8 + i] = rgba[i] }
        return pack
    }

    private static func unpackRGBA(_ pack: [UInt8]) -> (w: Int, h: Int, rgba: [UInt8])? {
        guard pack.count >= 8 else { return nil }
        let w = Int(pack[0]) | (Int(pack[1]) << 8)
        let h = Int(pack[2]) | (Int(pack[3]) << 8)
        guard w > 0, h > 0, pack.count >= 8 + w * h * 4 else { return nil }
        return (w, h, Array(pack[8...]))
    }

    /// 裁 content bbox，放大到 ≈320px 宽 / 安全盒高，居中贴 466（消除大片黑边细条）。
    private static func refitContentToSafeBox(_ pack: [UInt8]) -> [UInt8]? {
        guard let (sw, sh, rgba) = unpackRGBA(pack) else { return nil }
        var minX = sw, minY = sh, maxX = 0, maxY = 0
        for y in 0..<sh {
            for x in 0..<sw {
                let o = (y * sw + x) * 4
                let a = rgba[o + 3], r = rgba[o], g = rgba[o + 1], b = rgba[o + 2]
                if a < 20 || (r < 10 && g < 10 && b < 10) { continue }
                if x < minX { minX = x }; if y < minY { minY = y }
                if x > maxX { maxX = x }; if y > maxY { maxY = y }
            }
        }
        guard maxX > minX, maxY > minY else { return pack }
        let cw = maxX - minX + 1, ch = maxY - minY + 1
        let boxH = safeY1 - safeY0
        let scale = min(CGFloat(targetMaxW) / CGFloat(cw), CGFloat(boxH) / CGFloat(ch), 4)
        let nw = max(8, Int((CGFloat(cw) * scale).rounded()))
        let nh = max(8, Int((CGFloat(ch) * scale).rounded()))
        var out = [UInt8](repeating: 0, count: canvas * canvas * 4)
        let ox = (canvas - nw) / 2
        let oy = safeY0 + max(0, (boxH - nh) / 2)
        for dy in 0..<nh {
            let sy = minY + dy * ch / nh
            for dx in 0..<nw {
                let sx = minX + dx * cw / nw
                let so = (sy * sw + sx) * 4
                let dout = ((oy + dy) * canvas + (ox + dx)) * 4
                out[dout] = rgba[so]
                out[dout + 1] = rgba[so + 1]
                out[dout + 2] = rgba[so + 2]
                out[dout + 3] = rgba[so + 3]
            }
        }
        var packOut = [UInt8](repeating: 0, count: 8 + out.count)
        packOut[0] = UInt8(canvas & 0xFF); packOut[1] = UInt8((canvas >> 8) & 0xFF)
        packOut[2] = UInt8(canvas & 0xFF); packOut[3] = UInt8((canvas >> 8) & 0xFF)
        for i in 0..<out.count { packOut[8 + i] = out[i] }
        return packOut
    }

    private static func rgbaToRGB565(_ pack: [UInt8]) -> [UInt8]? {
        guard let (w, h, rgba) = unpackRGBA(pack), w == canvas, h == canvas else { return nil }
        var out = [UInt8](repeating: 0, count: canvas * canvas * 2)
        for i in 0..<(canvas * canvas) {
            let o = i * 4
            let a = rgba[o + 3], r = rgba[o], g = rgba[o + 1], b = rgba[o + 2]
            if a < 20 || (r < 8 && g < 8 && b < 8) { continue }
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
        // fix4：原来只在 0/1px 间抖，几乎看不出在动
        let bob = [0, 3, 6, 9, 6, 3][frame % 6]
        if bob != 0 { f = shift(f, dx: 0, dy: bob) }
        // 确认稿 sleep 图已含 Zzz；再叠相位漂浮增强多帧感
        drawZzz(&f, phase: frame)
        return f
    }

    private static func animateChase(_ base: [UInt8], frame: Int, vehicle: CenterSpriteVehicle) -> [UInt8] {
        var f = base
        // fix4：车身随帧起伏，让 6 帧有肉眼可见的位移
        let bob = [0, 2, 5, 8, 5, 2][frame % 6]
        if bob != 0 { f = shift(f, dx: 0, dy: bob) }
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
        // 放大主体后 Zzz 贴在安全盒上沿偏右
        let cx = canvas / 2 + 56
        let cy = safeY0 + 8 - (phase % 3) * 3
        let col: UInt16 = 0xC1BF
        drawOneZ(&f, x: cx, y: cy, s: 6, col: col)
        if phase % 2 == 0 { drawOneZ(&f, x: cx + 9, y: cy - 7, s: 5, col: col) }
        if phase % 3 == 0 { drawOneZ(&f, x: cx + 16, y: cy - 12, s: 4, col: col) }
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
        let baseX = canvas / 2 - 110
        let baseY = safeY1 - 36
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
        let r = small ? 6 : 10
        let cy = safeY1 - 40
        let cxs = [canvas / 2 - 48, canvas / 2 + 48]
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

    /// raw 已是顶左行序；直接建 CGImage，避免 UIGraphics 再翻一次。
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
        guard let ctx = CGContext(data: &rgba, width: canvas, height: canvas, bitsPerComponent: 8,
                                  bytesPerRow: canvas * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue),
              let cg = ctx.makeImage() else { return nil }
        return UIImage(cgImage: cg, scale: 1, orientation: .up)
    }
}
