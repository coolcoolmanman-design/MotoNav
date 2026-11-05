#if canImport(UIKit)
import UIKit
import Foundation

/// 把高德「路口放大图」转成圆屏能直接贴的索引图，分两段发（musicA.30）。
///
/// 为什么分两段：手机锁屏时 iOS 会把 BLE 连接间隔拉到 ~90ms，一片一个连接事件。
/// 272×174 的全图十五六片就是 1.4~2.1 秒（实测），骑行时手机必然锁屏，这段等待躲不掉。
/// 所以先发一张 136×87 的预览（片数只有 1/4，0.4~0.6 秒上屏，圆屏 2 倍放大、略糙但够认路），
/// 全图随后替换成清晰版。用户感受从「等两秒」变成「几乎立刻就出来」。
///
/// 顺带把调色板从 16 色降到 5 色（4 级灰 + 1 个彩色槽）：深色化之后本来就接近无彩，
/// 合并中间调让同色游程变长，包体再降三成左右 —— 片数少了，按上面那个 90ms/片算就是净赚。
///
/// 这里做四件事，圆屏只负责展开贴图：
/// 1. **深色化** —— 高德给的是浅底位图，直接上 AMOLED 夜间就是一块白板。灰阶整体反相
///    （白→接近黑、黑→浅灰），彩色（蓝色路线、红色禁令标志）保留色相只提亮。
/// 2. **自适应 5 色** —— 按本图实际亮度分布定 4 个灰阶，再加一个彩色槽取饱和像素的均值。
/// 3. **缩放** —— 全图压到 272×174，预览 136×87（同样 25:16，不裁切不变形）。
/// 4. **编码** —— nibble RLE（和栅格底图同一套），ver=2 表示 5 色。
enum CrossImageSender {
    static let W = 272
    static let H = 174
    static let PW = 136
    static let PH = 87
    static let bulkType = 9          // 全图
    static let prevType = 10         // 预览
    static let PAL_N = 5

    private static var lastHash: UInt64 = 0
    private static var lastSize = 0
    private static var lastId = 0
    private static var sent = 0
    // 路口图是「按需即时」的数据：用 .utility 会被系统限频，量化 + 缩放偶尔要几百毫秒。
    private static let queue = DispatchQueue(label: "motonav.cross", qos: .userInitiated)

    /// 返回一句给调试卡看的结论；真正的编码在后台队列做，完成后再回主线程发。
    @discardableResult
    static func send(_ image: UIImage) -> String {
        guard Settings.shared.crossOn else { return "路口放大图已关闭" }
        queue.async {
            guard let scene = deepScene(image) else { return }
            let pal = buildPalette(scene)

            // 先解预览：像素只有 1/4，算完立刻发，别等全图那道慢工
            let prevIdx = downsampleIndex(scene, pal: pal)
            let prevPacket = encode(prevIdx, w: PW, h: PH, pal: pal)

            let fullIdx = mapIndex(scene, pal: pal)
            let fullPacket = encode(fullIdx, w: W, h: H, pal: pal)
            let hash = fnv(fullIdx)

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

    /// 高德隐藏路口图（通过路口）→ 让圆屏退回小地图
    static func hide() {
        // 先把还没发完的分片作废，再让圆屏清图：否则剩下的分片会在圆屏上「重新拼出一张图」，
        // 表现为过了路口又冒出来一次（固件侧另有 2 秒静默窗兜底）。预览和全图是两个 type，都要清。
        BleLink.shared.cancelBulk(type: bulkType)
        BleLink.shared.cancelBulk(type: prevType)
        lastHash = 0; lastSize = 0        // 同一个路口下次再来还能重发
        BleLink.shared.send("#CXH")
    }

    // MARK: - 像素处理

    /// 深色化后的场景：每像素 3 字节 RGB（已反相/提亮），外加是否彩色的标记位
    private struct Scene {
        var rgb: [UInt8]              // W * H * 3
        var chroma: [UInt8]           // W * H，饱和度为 0 表示灰像素
    }

    private static func deepScene(_ image: UIImage) -> Scene? {
        guard let cg = image.cgImage else { return nil }
        var rgba = [UInt8](repeating: 0, count: W * H * 4)
        guard let ctx = CGContext(data: &rgba, width: W, height: H, bitsPerComponent: 8,
                                  bytesPerRow: W * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        // 先在画布上填一层深色底：高德的路口图很多是透明背景，不填的话那些像素 alpha=0，
        // 会被当成「透明区域」压成近黑 → 圆屏上整张卡片几乎全黑，看着就像「没有路口图」。
        // 底色和地图页地面同色（26,31,39），关掉图的时候边缘不会跳。
        ctx.setFillColor(CGColor(red: 26.0 / 255, green: 31.0 / 255, blue: 39.0 / 255, alpha: 1))
        ctx.fill(CGRect(x: 0, y: 0, width: W, height: H))
        ctx.interpolationQuality = .high
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: W, height: H))

        var rgb = [UInt8](repeating: 0, count: W * H * 3)
        var chroma = [UInt8](repeating: 0, count: W * H)
        for i in 0..<(W * H) {
            let r = Int(rgba[i * 4]), g = Int(rgba[i * 4 + 1])
            let b = Int(rgba[i * 4 + 2]), a = Int(rgba[i * 4 + 3])
            if a < 110 {                                  // 极少数仍透明的像素：用底色
                rgb[i * 3] = 26; rgb[i * 3 + 1] = 31; rgb[i * 3 + 2] = 39
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
                let v = Int(20.0 + (255.0 - lum) * 0.55)  // 灰阶反相：白(255)→20，黑(0)→160
                rr = v; gg = v; bb = v
            } else {
                let target = 96.0 + lum * 0.50            // 彩色保留色相，把明度抬到深底上也看得清
                let k = lum > 1 ? target / lum : 1.0
                rr = min(255, Int(Double(r) * k))
                gg = min(255, Int(Double(g) * k))
                bb = min(255, Int(Double(b) * k))
                chroma[i] = UInt8(min(255, ch))
            }
            rgb[i * 3] = UInt8(rr); rgb[i * 3 + 1] = UInt8(gg); rgb[i * 3 + 2] = UInt8(bb)
        }
        return Scene(rgb: rgb, chroma: chroma)
    }

    /// 自适应 5 色：灰阶按亮度四等分取每档均值，饱和像素取均值做第 5 个「彩色槽」。
    /// 比固定 16 色调色板更贴合本图（深色化后的亮度只落在 20~160 这一窄带），
    /// 也不会像 16 色那样把抗锯齿中间调拆成一堆几乎看不出差别的灰。
    private static func buildPalette(_ s: Scene) -> [(Int, Int, Int)] {
        // 四档边界按深色化后的实际范围 20~160 划
        let bounds = [20, 55, 85, 115, 161]
        var sum = [(Int, Int, Int)](repeating: (0, 0, 0), count: 4)
        var cnt = [Int](repeating: 0, count: 4)
        var cSum = (0, 0, 0), cCnt = 0

        for i in 0..<(W * H) {
            let r = Int(s.rgb[i * 3]), g = Int(s.rgb[i * 3 + 1]), b = Int(s.rgb[i * 3 + 2])
            if s.chroma[i] > 0 {
                cSum = (cSum.0 + r, cSum.1 + g, cSum.2 + b); cCnt += 1
                continue
            }
            let v = (r + g + b) / 3
            var k = 3
            for j in 0..<4 where v < bounds[j + 1] { k = j; break }
            sum[k] = (sum[k].0 + r, sum[k].1 + g, sum[k].2 + b); cnt[k] += 1
        }

        var pal: [(Int, Int, Int)] = []
        for k in 0..<4 {
            if cnt[k] > 0 {
                pal.append((sum[k].0 / cnt[k], sum[k].1 / cnt[k], sum[k].2 / cnt[k]))
            } else {
                // 空档（比如整张图没有特别亮的区域）：用该档的代表值兜底，保证单调不发灰
                let v = (bounds[k] + bounds[k + 1]) / 2
                pal.append((v, v, v))
            }
        }
        if cCnt > 0 {
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
