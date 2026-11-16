#if canImport(UIKit)
import UIKit
import Foundation

/// 把高德「路口放大图」转成圆屏能直接贴的索引图，分两段发（musicA.30），并按昼夜出两套配色（musicA.32）。
///
/// 为什么分两段：手机锁屏时 iOS 会把 BLE 连接间隔拉到 ~90ms，一片一个连接事件。
/// 272×150 的全图十五六片就是 1.4~2.1 秒（实测），骑行时手机必然锁屏，这段等待躲不掉。
/// 所以先发一张 136×75 的预览（片数只有 1/4，0.4~0.6 秒上屏，圆屏 2 倍放大、略糙但够认路），
/// 全图随后替换成清晰版。用户感受从「等两秒」变成「几乎立刻就出来」。
///
/// 顺带把调色板从 16 色降到 5 色（4 级灰 + 1 个彩色槽）：量化后本就接近无彩，
/// 合并中间调让同色游程变长，包体再降三成左右 —— 片数少了，按上面那个 90ms/片算就是净赚。
///
/// 这里做四件事，圆屏只负责展开贴图：
/// 1. **配色归一** —— 高德给的是浅底位图。夜间灰阶反相（白→近黑、黑→浅灰）、底铺深色，
///    彩色提亮到深底上也看得清；白天保留原图的浅底深线明暗、底铺浅色、彩色保持原色。
///    固件本来就有 `THEMES` / `THEMES_NIGHT` + `nightOn`（App 按日出日落发 `#n,1/0`），
///    路口图以前是绕开这套机制的「永久夜间」，白天浅色地图上贴着一块深色浮窗，现在补齐。
/// 2. **自适应 5 色** —— 按本图实际亮度分布定 4 个灰阶（昼夜亮度范围差很多，不能写死边界），
///    再加一个彩色槽取最饱和像素的均值（高德图里除了路线还有绿地/图标/禁令牌，
///    全取均值会变脏色；只吃高饱和部分基本就只剩路线本体）。
/// 3. **缩放** —— 全图压到 272×150，预览 136×75（正好 1/2，不裁切不变形）。
/// 4. **编码** —— nibble RLE（和栅格底图同一套），ver=2 表示 5 色。
enum CrossImageSender {
    static let W = 272
    static let H = 150
    static let PW = 136
    static let PH = 75
    static let bulkType = 9          // 全图
    static let prevType = 10         // 预览
    static let PAL_N = 5

    private static var lastHash: UInt64 = 0
    private static var lastSize = 0
    private static var lastId = 0
    private static var sent = 0
    private static var lastImage: UIImage?   // 昼夜切换时按新主题重发这一张
    // musicA.35 / iOS 0.16.2.32：相邻很近的两个路口，高德的 show(新图) / hide(旧图)
    // 回调可能只隔几百毫秒甚至乱序。旧版 hide() 立刻 cancelBulk(9/10) + 发 #CXH，
    // 会把刚入队的第二张图一起清掉。现在延迟 400ms 再真正隐藏，期间来了新 send()
    // 就取消隐藏、直接换图（连续路口变成无缝切换，不再退回小地图又冒出来）。
    // send/hide 都由高德主线程回调进入，pendingHide 只在该线程读写。
    private static var pendingHide: DispatchWorkItem?
    // 路口图是「按需即时」的数据：用 .utility 会被系统限频，量化 + 缩放偶尔要几百毫秒。
    private static let queue = DispatchQueue(label: "motonav.cross", qos: .userInitiated)

    /// 返回一句给调试卡看的结论；真正的编码在后台队列做，完成后再回主线程发。
    @discardableResult
    static func send(_ image: UIImage) -> String {
        guard Settings.shared.crossOn else { return "路口放大图已关闭" }
        guard !Settings.crossHiddenThemes.contains(Settings.shared.theme) else { return "该样式下路口放大图自动隐藏" }
        cancelPendingHide()                        // 新图到了：上一个路口的待隐藏作废，直接换图
        let night = Settings.shared.paletteNight    // 主线程取，别跨线程读 @Published
        lastImage = image
        queue.async {
            guard let scene = deepScene(image, night: night) else { return }
            let pal = buildPalette(scene)

            // 先解预览：像素只有 1/4，算完立刻发，别等全图那道慢工
            let prevIdx = downsampleIndex(scene, pal: pal)
            let prevPacket = encode(prevIdx, w: PW, h: PH, pal: pal)

            let fullIdx = mapIndex(scene, pal: pal)
            let fullPacket = encode(fullIdx, w: W, h: H, pal: pal)
            // 昼夜也算进指纹：日出/日落换主题时，同一张路口图必须重发，
            // 否则 hash 命中会跳过，圆屏上一直留着上一个色调的旧图。
            let hash = fnv(fullIdx) ^ (night ? 0x9e3779b97f4a7c15 : 0)

            DispatchQueue.main.async {
                if hash == lastHash && fullPacket.count == lastSize { return }   // 同一个路口重复推送，跳过
                lastHash = hash; lastSize = fullPacket.count
                lastId = (lastId + 1) & 0xFF
                sent += 1
                // 两次入队顺序有讲究：全图先入（prio 10）但**先不开闸**，预览再入（prio 20）才 pump，
                // 这样队列是 [预览 5 片][全图 15 片][底图…]，预览一定在最前面，不会被全图抢了先。
                BleLink.shared.sendBulk(type: bulkType, id: lastId, data: fullPacket, prio: 10, pumpNow: false)
                BleLink.shared.sendBulk(type: prevType, id: lastId, data: prevPacket, prio: 20, pumpNow: true)
                print("[Cross] 已下发 #\(sent) 预览\(prevPacket.count / 1024)KB 全图\(fullPacket.count / 1024)KB id=\(lastId)")
                NaviBridge.shared.crossDebug += " | 已下发 预览\(prevPacket.count / 1024)KB 全图\(fullPacket.count / 1024)KB id=\(lastId)"
            }
        }
        return "路口图处理中"
    }

    /// 夜间配色切换（日出日落自动切 / 设置里手动切）时调用：把还在显示的那张图按新主题重发一遍。
    static func onPaletteChanged() {
        DispatchQueue.main.async {
            guard let img = CrossImageSender.lastImage else { return }
            _ = CrossImageSender.send(img)
        }
    }

    /// 高德隐藏路口图（通过路口）→ 延迟 400ms 让圆屏退回小地图。
    /// 这 400ms 是给「相邻近路口的新图」留的窗口：期间 send() 会取消这次隐藏。
    static func hide() {
        cancelPendingHide()
        let work = DispatchWorkItem {
            // 先把还没发完的分片作废，再让圆屏清图：否则剩下的分片会在圆屏上「重新拼出一张图」。
            // 预览和全图是两个 type，都要清。固件侧静默窗只丢同代残包，新代仍会放行。
            BleLink.shared.cancelBulk(type: bulkType)
            BleLink.shared.cancelBulk(type: prevType)
            lastHash = 0; lastSize = 0; lastImage = nil    // 已过路口就别留着，免得昼夜切换时又发出来
            BleLink.shared.send("#CXH")
            pendingHide = nil
        }
        pendingHide = work
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.4, execute: work)
    }

    private static func cancelPendingHide() {
        pendingHide?.cancel()
        pendingHide = nil
    }

    // MARK: - 像素处理

    /// 配色归一后的场景：每像素 3 字节 RGB，外加色差幅度（0 表示灰像素）
    private struct Scene {
        var rgb: [UInt8]              // W * H * 3
        var chroma: [UInt8]           // W * H
    }

    private static func deepScene(_ image: UIImage, night: Bool) -> Scene? {
        guard let cg = image.cgImage else { return nil }
        // 高德路口图很多是透明背景，必须先铺底；底色要跟圆屏地图主题走，
        // 夜间用地图页地面色，白天用接近纸面的浅色，关掉图的时候边缘不跳。
        let bg: (UInt8, UInt8, UInt8) = night ? (26, 31, 39) : (244, 244, 240)
        var rgba = [UInt8](repeating: 0, count: W * H * 4)
        guard let ctx = CGContext(data: &rgba, width: W, height: H, bitsPerComponent: 8,
                                  bytesPerRow: W * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.setFillColor(CGColor(red: Double(bg.0) / 255, green: Double(bg.1) / 255,
                                 blue: Double(bg.2) / 255, alpha: 1))
        ctx.fill(CGRect(x: 0, y: 0, width: W, height: H))
        ctx.interpolationQuality = .high
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: W, height: H))

        var rgb = [UInt8](repeating: 0, count: W * H * 3)
        var chroma = [UInt8](repeating: 0, count: W * H)
        for i in 0..<(W * H) {
            let r = Int(rgba[i * 4]), g = Int(rgba[i * 4 + 1])
            let b = Int(rgba[i * 4 + 2]), a = Int(rgba[i * 4 + 3])
            if a < 110 {                                  // 极少数仍透明的像素：用底色
                rgb[i * 3] = bg.0; rgb[i * 3 + 1] = bg.1; rgb[i * 3 + 2] = bg.2
                continue
            }
            let mx = max(r, max(g, b)), mn = min(r, min(g, b))
            let ch = mx - mn
            let lum = 0.299 * Double(r) + 0.587 * Double(g) + 0.114 * Double(b)
            // 判定「灰」必须用绝对色差：深色文字的 R/G/B 只差十几个数，相对饱和度却有 0.18，
            // 用 rel-sat 会把黑字误判成彩色 → 提亮后只剩一团灰蓝，字反而看不清。
            let isGray = ch < 26 || (mx > 0 && Double(ch) / Double(mx) < 0.14)
            var rr = r, gg = g, bb = b
            if isGray {
                let v: Int
                if night {
                    v = Int(20.0 + (255.0 - lum) * 0.55)   // 灰阶反相：白(255)→20，黑(0)→160
                } else {
                    v = Int(24.0 + lum * 0.84)             // 白天：浅底深线，保留高德原图的明暗
                }
                rr = v; gg = v; bb = v
            } else {
                if night {
                    let target = 96.0 + lum * 0.50        // 彩色保留色相，把明度抬到深底上也看得清
                    let k = lum > 1 ? target / lum : 1.0
                    rr = min(255, Int(Double(r) * k))
                    gg = min(255, Int(Double(g) * k))
                    bb = min(255, Int(Double(b) * k))
                }
                // 白天彩色保持原色：路线本来就是鲜明的蓝/绿，浅底上已经够清楚，再提亮反而发白。
                chroma[i] = UInt8(min(255, ch))
            }
            rgb[i * 3] = UInt8(rr); rgb[i * 3 + 1] = UInt8(gg); rgb[i * 3 + 2] = UInt8(bb)
        }
        return Scene(rgb: rgb, chroma: chroma)
    }

    /// 自适应 5 色：灰阶按本图实际亮度四等分取每档均值，第 5 个彩色槽只取高饱和像素的均值。
    /// 夜间反相后亮度落在 20~160，白天浅底落在 24~238，写死一组边界会让其中一套色带挤成一团，
    /// 所以边界改为按本图 min/max 动态计算。
    private static func buildPalette(_ s: Scene) -> [(Int, Int, Int)] {
        var lo = 255, hi = 0
        for i in 0..<(W * H) where s.chroma[i] == 0 {
            let v = (Int(s.rgb[i * 3]) + Int(s.rgb[i * 3 + 1]) + Int(s.rgb[i * 3 + 2])) / 3
            if v < lo { lo = v }
            if v > hi { hi = v }
        }
        if hi - lo < 32 { lo = 0; hi = 255 }              // 退化保护：整张图几乎一个亮度
        let step = Double(hi - lo + 1) / 4.0

        var sum = [(Int, Int, Int)](repeating: (0, 0, 0), count: 4)
        var cnt = [Int](repeating: 0, count: 4)
        var cSum = (0, 0, 0), cCnt = 0
        var strongSum = (0, 0, 0), strongCnt = 0

        for i in 0..<(W * H) {
            let r = Int(s.rgb[i * 3]), g = Int(s.rgb[i * 3 + 1]), b = Int(s.rgb[i * 3 + 2])
            if s.chroma[i] > 0 {
                cSum = (cSum.0 + r, cSum.1 + g, cSum.2 + b); cCnt += 1
                // 彩色槽锁给最饱和的那部分像素：绿地 / POI 图标 / 禁令牌的色差一般 < 70，
                // 路线本体（纯蓝或纯绿）稳稳在阈值之上，这样第 5 色就是「走哪条」。
                if s.chroma[i] >= 70 {
                    strongSum = (strongSum.0 + r, strongSum.1 + g, strongSum.2 + b); strongCnt += 1
                }
                continue
            }
            let v = (r + g + b) / 3
            let rel = Double(v - lo)
            var k = 3
            for j in 0..<4 where rel < step * Double(j + 1) { k = j; break }
            sum[k] = (sum[k].0 + r, sum[k].1 + g, sum[k].2 + b); cnt[k] += 1
        }

        var pal: [(Int, Int, Int)] = []
        for k in 0..<4 {
            if cnt[k] > 0 {
                pal.append((sum[k].0 / cnt[k], sum[k].1 / cnt[k], sum[k].2 / cnt[k]))
            } else {
                // 空档（比如整张图没有特别亮的区域）：用该档的代表值兜底，保证单调不发灰
                let v = lo + Int(step * (Double(k) + 0.5))
                pal.append((v, v, v))
            }
        }
        if strongCnt > 0 {
            pal.append((strongSum.0 / strongCnt, strongSum.1 / strongCnt, strongSum.2 / strongCnt))
        } else if cCnt > 0 {
            pal.append((cSum.0 / cCnt, cSum.1 / cCnt, cSum.2 / cCnt))
        } else {
            pal.append(pal[3])                      // 没有彩色就复制最亮档，凑满 5 个
        }
        // 亮度单调递增，避免量化后色带倒挂
        pal.sort { ($0.0 + $0.1 + $0.2) < ($1.0 + $1.1 + $1.2) }
        return pal
    }

    private static func nearest(_ r: Int, _ g: Int, _ b: Int, _ pal: [(Int, Int, Int)]) -> UInt8 {
        var best = 0, bestD = Int.max
        for i in 0..<pal.count {
            let c = pal[i]
            let dr = r - c.0, dg = g - c.1, db = b - c.2
            let d = dr * dr * 2 + dg * dg * 4 + db * db     // 近似人眼权重，绿>红>蓝
            if d < bestD { bestD = d; best = i }
        }
        return UInt8(best)
    }

    private static func mapIndex(_ s: Scene, pal: [(Int, Int, Int)]) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: W * H)
        for i in 0..<(W * H) {
            out[i] = nearest(Int(s.rgb[i * 3]), Int(s.rgb[i * 3 + 1]), Int(s.rgb[i * 3 + 2]), pal)
        }
        return out
    }

    /// 预览：2×2 取平均再量化（正好对应固件侧的 2 倍最近邻放大）。
    /// 取平均而不是抽点，是为了让细线不会整条漏掉 —— 抽点会让 1px 的道路时有时无。
    private static func downsampleIndex(_ s: Scene, pal: [(Int, Int, Int)]) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: PW * PH)
        for y in 0..<PH {
            for x in 0..<PW {
                var r = 0, g = 0, b = 0
                for dy in 0..<2 {
                    let sy = y * 2 + dy
                    guard sy < H else { continue }
                    for dx in 0..<2 {
                        let sx = x * 2 + dx
                        guard sx < W else { continue }
                        let i = (sy * W + sx) * 3
                        r += Int(s.rgb[i]); g += Int(s.rgb[i + 1]); b += Int(s.rgb[i + 2])
                    }
                }
                out[y * PW + x] = nearest(r / 4, g / 4, b / 4, pal)
            }
        }
        return out
    }

    // MARK: - 打包

    /// 'X' | ver=2 | u16 w | u16 h | 16×u16 RGB565（前 PAL_N 个有效）| RLE
    /// RLE：高 4 位 n，n<15 → n+1 个；n=15 → 16+下一字节 个。
    private static func encode(_ idx: [UInt8], w: Int, h: Int, pal: [(Int, Int, Int)]) -> Data {
        var b = [UInt8]()
        b.reserveCapacity(idx.count / 4 + 64)
        b.append(0x58); b.append(2)
        b.append(UInt8(w & 0xFF)); b.append(UInt8(w >> 8))
        b.append(UInt8(h & 0xFF)); b.append(UInt8(h >> 8))
        // 调色板永远占 32 字节，只有前 PAL_N 个有效
        for i in 0..<16 {
            let c = i < pal.count ? pal[i] : (0, 0, 0)
            let r = UInt16(c.0), g = UInt16(c.1), bl = UInt16(c.2)
            let v: UInt16 = (r >> 3) << 11 | (g >> 2) << 5 | (bl >> 3)
            b.append(UInt8(v & 0xFF)); b.append(UInt8(v >> 8))
        }
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
