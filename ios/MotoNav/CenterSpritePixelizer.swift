import Foundation
import UIKit

/// 组合动画 · 性别（男生 / 女生）
enum CenterSpriteGender: Int, CaseIterable, Identifiable {
    case male = 0, female
    var id: Int { rawValue }
    var title: String { self == .male ? "男生" : "女生" }
}

/// 组合动画 · 八车型（无相册上传）
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
    /// 封闭四轮：舱内构图、不戴头盔
    var isCabinCar: Bool {
        switch self {
        case .sedan, .suv, .supercar: return true
        default: return false
        }
    }
    var usesHelmet: Bool {
        switch self {
        case .motorcycle, .scooter, .sportbike: return true
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
    var exhaustSharp: Bool {
        self == .sportbike || self == .supercar
    }
    var exhaustSoft: Bool { self == .scooter }
}

/// 中央槽「组合」预制：男生/女生 × 八车型 → 厂编密度两态多帧（sleep Zzz / chase 挥手·尾气·踏频）。
/// 无相册上传、无 Vision 抠图。
enum CenterSpritePixelizer {
    static let canvas = 466
    /// 对齐厂编 GTA ≈160px 宽：逻辑格 × cell3
    static let cell = 3
    static let contentMaxW = 162
    static let safeY0 = 150
    static let safeY1 = 348
    static let sleepFrames = 6
    static let chaseFrames = 6

    /// 逻辑画布（格）
    private static let gw = 54
    private static let gh = 54

    struct Result {
        var sleepRGB565: [[UInt8]]
        var chaseRGB565: [[UInt8]]
        var previewSleep: [UIImage]
        var previewChase: [UIImage]
    }

    /// 预制组合：性别 × 车型 → sleep/chase 帧。
    static func build(gender: CenterSpriteGender, vehicle: CenterSpriteVehicle) -> Result {
        var sleep: [[UInt8]] = []
        var chase: [[UInt8]] = []
        var prevS: [UIImage] = []
        var prevC: [UIImage] = []
        for i in 0..<sleepFrames {
            let f = compose(gender: gender, vehicle: vehicle, online: false, frame: i)
            sleep.append(f)
            if let img = rgb565ToUIImage(f) { prevS.append(img) }
        }
        for i in 0..<chaseFrames {
            let f = compose(gender: gender, vehicle: vehicle, online: true, frame: i)
            chase.append(f)
            if let img = rgb565ToUIImage(f) { prevC.append(img) }
        }
        return Result(sleepRGB565: sleep, chaseRGB565: chase, previewSleep: prevS, previewChase: prevC)
    }

    // MARK: - Compose one frame

    private static func compose(gender: CenterSpriteGender, vehicle: CenterSpriteVehicle,
                                online: Bool, frame: Int) -> [UInt8] {
        var grid = [UInt16](repeating: 0, count: gw * gh)
        let bob = (!online && frame % 2 == 1) ? 1 : 0

        if vehicle.isCabinCar {
            drawCabinCar(&grid, vehicle: vehicle, gender: gender, online: online, frame: frame, bob: bob)
        } else if vehicle.usesPedal {
            drawCycle(&grid, vehicle: vehicle, gender: gender, online: online, frame: frame, bob: bob)
        } else {
            drawMoto(&grid, vehicle: vehicle, gender: gender, online: online, frame: frame, bob: bob)
        }

        if !online { drawZzzGrid(&grid, phase: frame) }
        else if vehicle.usesExhaust { drawExhaustGrid(&grid, vehicle: vehicle, phase: frame) }

        return blitGrid(grid)
    }

    // MARK: - Palette

    private static let outline: UInt16 = 0x0841
    private static let skin: UInt16 = 0xFDB5
    private static let hairDark: UInt16 = 0x2104
    private static let hairBrown: UInt16 = 0x8200
    private static let jacketBlue: UInt16 = 0x2A9F
    private static let jacketPink: UInt16 = 0xF9B3
    private static let pants: UInt16 = 0x3186
    private static let jeans: UInt16 = 0x3A0F
    private static let helmet: UInt16 = 0x1082
    private static let visor: UInt16 = 0x0618
    private static let chrome: UInt16 = 0xC618
    private static let metal: UInt16 = 0x7BEF
    private static let tire: UInt16 = 0x1082
    private static let bodyDark: UInt16 = 0x19AE
    private static let bodySilver: UInt16 = 0xAD55
    private static let bodyRed: UInt16 = 0xD0E4
    private static let bodyOrange: UInt16 = 0xFC00
    private static let headlight: UInt16 = 0xFD20
    private static let window: UInt16 = 0x3C57
    private static let smoke: UInt16 = 0x8410
    private static let zzzCol: UInt16 = 0xC1BF
    private static let seat: UInt16 = 0x3186
    private static let frameOrange: UInt16 = 0xFC00

    private static func put(_ g: inout [UInt16], _ x: Int, _ y: Int, _ c: UInt16) {
        guard x >= 0, y >= 0, x < gw, y < gh, c != 0 else { return }
        g[y * gw + x] = c
    }

    private static func fill(_ g: inout [UInt16], _ x0: Int, _ y0: Int, _ w: Int, _ h: Int, _ c: UInt16) {
        for y in y0..<(y0 + h) {
            for x in x0..<(x0 + w) { put(&g, x, y, c) }
        }
    }

    private static func outlineBlob(_ g: inout [UInt16]) {
        var edged = g
        for y in 0..<gh {
            for x in 0..<gw {
                if g[y * gw + x] == 0 { continue }
                var edge = false
                for oy in -1...1 {
                    for ox in -1...1 {
                        if ox == 0 && oy == 0 { continue }
                        let nx = x + ox, ny = y + oy
                        if nx < 0 || ny < 0 || nx >= gw || ny >= gh || g[ny * gw + nx] == 0 {
                            edge = true; break
                        }
                    }
                    if edge { break }
                }
                if edge { edged[y * gw + x] = outline }
            }
        }
        g = edged
    }

    // MARK: - Motorcycle / scooter / sportbike

    private static func drawMoto(_ g: inout [UInt16], vehicle: CenterSpriteVehicle,
                                 gender: CenterSpriteGender, online: Bool, frame: Int, bob: Int) {
        let ox = 4, oy = 8 + bob
        // wheels
        let wr = vehicle == .sportbike ? 7 : 8
        drawTire(&g, cx: ox + 12, cy: oy + 40, r: wr)
        drawTire(&g, cx: ox + 40, cy: oy + 40, r: wr)
        if online {
            drawSpoke(&g, cx: ox + 12, cy: oy + 40, r: wr - 2, phase: frame)
            drawSpoke(&g, cx: ox + 40, cy: oy + 40, r: wr - 2, phase: frame)
        }

        // body / tank / seat by type
        let bodyCol: UInt16 = {
            switch vehicle {
            case .scooter: return bodySilver
            case .sportbike: return bodyRed
            default: return bodyDark
            }
        }()
        if vehicle == .scooter {
            fill(&g, ox + 14, oy + 28, 24, 6, bodyCol)
            fill(&g, ox + 18, oy + 22, 14, 8, bodyCol)
            fill(&g, ox + 12, oy + 18, 6, 16, chrome) // floorboard stem
            fill(&g, ox + 10, oy + 32, 12, 3, chrome) // deck
        } else if vehicle == .sportbike {
            fill(&g, ox + 16, oy + 26, 22, 8, bodyCol)
            fill(&g, ox + 20, oy + 20, 16, 8, bodyCol)
            fill(&g, ox + 34, oy + 18, 8, 6, bodyCol) // tail
            fill(&g, ox + 14, oy + 22, 6, 5, window) // windscreen
        } else {
            fill(&g, ox + 16, oy + 26, 20, 8, bodyCol)
            fill(&g, ox + 18, oy + 22, 14, 6, bodyCol)
            fill(&g, ox + 22, oy + 28, 10, 4, seat)
        }
        // engine / exhaust chrome
        fill(&g, ox + 22, oy + 34, 10, 4, metal)
        fill(&g, ox + 8, oy + 34, 8, 2, chrome)
        put(&g, ox + 42, oy + 30, headlight)
        put(&g, ox + 43, oy + 30, headlight)

        // rider
        let jacket = gender == .male ? jacketBlue : jacketPink
        let pant = gender == .male ? pants : jeans
        let hx = ox + 24, hy = oy + 6
        drawRiderHead(&g, hx: hx, hy: hy, gender: gender, helmet: true, online: online, wave: online, phase: frame)
        fill(&g, hx - 3, hy + 8, 8, 10, jacket) // torso
        if online {
            // waving right arm
            let lift = (frame % 3)
            fill(&g, hx + 5, hy + 6 - lift, 3, 7, jacket)
            fill(&g, hx + 7, hy + 4 - lift, 3, 3, skin)
        } else {
            fill(&g, hx - 5, hy + 10, 3, 6, jacket) // arms folded
            fill(&g, hx + 4, hy + 10, 3, 6, jacket)
        }
        fill(&g, hx - 2, hy + 17, 3, 8, pant)
        fill(&g, hx + 2, hy + 17, 3, 8, pant)
        outlineBlob(&g)
    }

    // MARK: - Cabin cars (sedan / SUV / supercar) — in-cabin, no helmet

    private static func drawCabinCar(_ g: inout [UInt16], vehicle: CenterSpriteVehicle,
                                     gender: CenterSpriteGender, online: Bool, frame: Int, bob: Int) {
        let ox = 2, oy = 12 + bob
        let bodyCol: UInt16 = {
            switch vehicle {
            case .suv: return bodyDark
            case .supercar: return bodyRed
            default: return bodySilver
            }
        }()
        let carH = vehicle == .suv ? 22 : (vehicle == .supercar ? 14 : 16)
        let carY = oy + (vehicle == .suv ? 14 : 18)
        let roofH = vehicle == .suv ? 10 : (vehicle == .supercar ? 6 : 8)

        // wheels
        drawTire(&g, cx: ox + 12, cy: carY + carH - 2, r: 6)
        drawTire(&g, cx: ox + 40, cy: carY + carH - 2, r: 6)
        if online {
            drawSpoke(&g, cx: ox + 12, cy: carY + carH - 2, r: 4, phase: frame)
            drawSpoke(&g, cx: ox + 40, cy: carY + carH - 2, r: 4, phase: frame)
        }

        // body
        fill(&g, ox + 6, carY, 42, carH, bodyCol)
        // cabin / roof
        fill(&g, ox + 14, carY - roofH, 24, roofH + 2, bodyCol)
        fill(&g, ox + 16, carY - roofH + 1, 20, roofH - 1, window)
        // hood / trunk accents
        if vehicle == .supercar {
            fill(&g, ox + 8, carY + 2, 10, 4, bodyCol)
            fill(&g, ox + 36, carY + 2, 10, 3, bodyCol)
        }
        put(&g, ox + 8, carY + 4, headlight)
        put(&g, ox + 9, carY + 4, headlight)

        // person in cabin (upper body out window) — no helmet
        let jacket = gender == .male ? jacketBlue : jacketPink
        let hx = ox + 22, hy = carY - roofH - 2
        drawRiderHead(&g, hx: hx, hy: hy, gender: gender, helmet: false, online: online, wave: online, phase: frame)
        fill(&g, hx - 3, hy + 8, 8, 6, jacket)
        if online {
            let lift = frame % 3
            fill(&g, hx + 5, hy + 5 - lift, 3, 6, jacket)
            fill(&g, hx + 7, hy + 3 - lift, 3, 3, skin)
        } else {
            // resting on window sill
            fill(&g, hx - 4, hy + 10, 10, 3, jacket)
            fill(&g, hx - 2, hy + 12, 4, 2, skin)
        }
        outlineBlob(&g)
    }

    // MARK: - Bicycle / folding

    private static func drawCycle(_ g: inout [UInt16], vehicle: CenterSpriteVehicle,
                                  gender: CenterSpriteGender, online: Bool, frame: Int, bob: Int) {
        let ox = 4, oy = 6 + bob
        let small = vehicle == .foldingBike
        let wr = small ? 6 : 8
        drawTire(&g, cx: ox + 12, cy: oy + 42, r: wr)
        drawTire(&g, cx: ox + 40, cy: oy + 42, r: wr)
        if online {
            drawSpoke(&g, cx: ox + 12, cy: oy + 42, r: wr - 2, phase: frame)
            drawSpoke(&g, cx: ox + 40, cy: oy + 42, r: wr - 2, phase: frame)
        }
        // frame
        let fc = small ? frameOrange : chrome
        for t in 0..<3 {
            put(&g, ox + 18 + t, oy + 28, fc)
            put(&g, ox + 22 + t, oy + 22, fc)
            put(&g, ox + 28 + t, oy + 28, fc)
            put(&g, ox + 24, oy + 22 + t, fc)
        }
        fill(&g, ox + 20, oy + 30, 12, 2, fc)
        fill(&g, ox + 24, oy + 26, 4, 2, seat)

        let jacket = gender == .male ? jacketBlue : jacketPink
        let pant = gender == .male ? pants : jeans
        let hx = ox + 24, hy = oy + 4
        drawRiderHead(&g, hx: hx, hy: hy, gender: gender, helmet: false, online: online, wave: online, phase: frame)
        fill(&g, hx - 3, hy + 8, 8, 10, jacket)
        if online {
            let lift = frame % 3
            fill(&g, hx + 5, hy + 6 - lift, 3, 7, jacket)
            fill(&g, hx + 7, hy + 4 - lift, 3, 3, skin)
        } else {
            fill(&g, hx - 5, hy + 10, 3, 6, jacket)
            fill(&g, hx + 4, hy + 10, 3, 6, jacket)
        }
        // pedaling legs
        let pa = online ? Double(frame) * (.pi / 3) : 0.6
        let lx = hx - 1 + Int((cos(pa) * 4).rounded())
        let ly = hy + 18 + Int((sin(pa) * 3).rounded())
        fill(&g, lx, ly, 3, 8, pant)
        fill(&g, hx + 2, hy + 18, 3, 8, pant)
        outlineBlob(&g)
    }

    // MARK: - Shared parts

    private static func drawTire(_ g: inout [UInt16], cx: Int, cy: Int, r: Int) {
        for y in (cy - r)...(cy + r) {
            for x in (cx - r)...(cx + r) {
                let dx = x - cx, dy = y - cy
                if dx * dx + dy * dy <= r * r { put(&g, x, y, tire) }
                if dx * dx + dy * dy <= (r - 2) * (r - 2) { put(&g, x, y, metal) }
            }
        }
    }

    private static func drawSpoke(_ g: inout [UInt16], cx: Int, cy: Int, r: Int, phase: Int) {
        let ang = Double(phase) * (.pi / 3)
        let x2 = cx + Int((cos(ang) * Double(r)).rounded())
        let y2 = cy + Int((sin(ang) * Double(r)).rounded())
        put(&g, cx, cy, chrome)
        put(&g, x2, y2, chrome)
        put(&g, cx - (x2 - cx), cy - (y2 - cy), chrome)
    }

    private static func drawRiderHead(_ g: inout [UInt16], hx: Int, hy: Int,
                                      gender: CenterSpriteGender, helmet: Bool,
                                      online: Bool, wave: Bool, phase: Int) {
        if helmet {
            fill(&g, hx - 4, hy, 10, 8, CenterSpritePixelizer.helmet)
            fill(&g, hx - 2, hy + 3, 6, 2, visor)
            if !online {
                // sleepy slits slits
                put(&g, hx - 1, hy + 4, outline)
                put(&g, hx + 2, hy + 4, outline)
            }
        } else {
            let hair = gender == .male ? hairDark : hairBrown
            fill(&g, hx - 3, hy + 2, 8, 7, skin)
            fill(&g, hx - 4, hy, 10, 4, hair)
            if gender == .female {
                fill(&g, hx - 5, hy + 2, 2, 6, hair) // side hair
                fill(&g, hx + 5, hy + 2, 2, 6, hair)
            }
            if online {
                put(&g, hx - 1, hy + 5, outline)
                put(&g, hx + 2, hy + 5, outline)
            } else {
                // closed eyes
                put(&g, hx - 1, hy + 5, outline)
                put(&g, hx, hy + 5, outline)
                put(&g, hx + 2, hy + 5, outline)
                put(&g, hx + 3, hy + 5, outline)
            }
        }
        _ = wave; _ = phase
    }

    private static func drawZzzGrid(_ g: inout [UInt16], phase: Int) {
        let cx = 38, cy = 6 - (phase % 3)
        drawOneZ(&g, x: cx, y: cy, s: 4)
        if phase % 2 == 0 { drawOneZ(&g, x: cx + 6, y: cy - 4, s: 3) }
        if phase % 3 == 0 { drawOneZ(&g, x: cx + 10, y: cy - 7, s: 2) }
    }

    private static func drawOneZ(_ g: inout [UInt16], x: Int, y: Int, s: Int) {
        for i in 0..<s {
            put(&g, x + i, y, zzzCol)
            put(&g, x + s - 1 - i, y + i, zzzCol)
            put(&g, x + i, y + s - 1, zzzCol)
        }
    }

    private static func drawExhaustGrid(_ g: inout [UInt16], vehicle: CenterSpriteVehicle, phase: Int) {
        let baseX = 4
        let baseY = 36
        let n: Int
        if vehicle.exhaustSharp { n = 3 + phase % 3 }
        else if vehicle.exhaustSoft { n = 1 + phase % 2 }
        else { n = 2 + phase % 2 }
        for i in 0..<n {
            let ox = baseX - i * 2 - (phase % 2)
            let oy = baseY - i * 2 - (phase % 3)
            fill(&g, ox, oy, 2 + i / 2, 2 + i / 2, smoke)
        }
    }

    // MARK: - Blit logic grid → 466 RGB565 LE

    private static func blitGrid(_ grid: [UInt16]) -> [UInt8] {
        let pw = gw * cell, ph = gh * cell
        let ox = (canvas - pw) / 2
        let oy = safeY0 + max(0, ((safeY1 - safeY0) - ph) / 2)
        var out = [UInt8](repeating: 0, count: canvas * canvas * 2)
        for y in 0..<ph {
            let gy = y / cell
            for x in 0..<pw {
                let gx = x / cell
                let c = grid[gy * gw + gx]
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
