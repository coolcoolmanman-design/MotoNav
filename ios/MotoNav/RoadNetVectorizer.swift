import Foundation

/// 把栅格底图的道路标签层（RasterPacket 的 0/1/2/3 索引）矢量化成折线要素，
/// 走固件现成的 bulk type 1 通道给「矢量简图」画周边路网。
///
/// 为什么不直接下发栅格：栅格包是 100 KB 量级，BLE 实际吞吐约 5 KB/s，
/// 导航中持续发会挤掉路线和路口图。要素包只有 10 KB 量级，而且线条不随缩放糊掉。
///
/// 为什么不用高德的矢量数据：AMap 侧只有截图，底层道路就是一个分类索引图
/// （PX_ROAD = 1），所以"真实路网"只能从这张标签图里把中心线抽出来。
enum RoadNetVectorizer {

    /// 取多大范围（半径，米）。圆屏视野约 480~800 m，600 m 出头足够，再远也看不见。
    static let windowM = 620.0

    /// 骨架化前的降采样步长（原图 2 m/px × 2 = 4 m/px）。
    /// 4 m 的骨架偏差在 466 px 圆屏上只有 1~2 px，肉眼不可见，但细化速度快 4 倍。
    static let step = 2

    struct Output {
        let data: Data
        let lines: Int
        let points: Int
    }

    /// 一条抽出来的道路折线（平面米坐标）+ 量出来的宽度（米）
    private struct RoadLine {
        var pts: [Double]
        var width: Double
    }

    // MARK: - 入口

    /// - Parameters:
    ///   - idx: `RasterPacket.decode` 出来的 1200² 标签图（`MapGeo.PX_ROAD` = 1）
    ///   - originX/originY: 栅格左上角平面坐标（米）；行号增大对应 Y 减小
    static func build(frameId: Int, idx: [UInt8], w: Int, h: Int,
                      originX: Double, originY: Double, mpp: Double,
                      maxBytes: Int, maxFeat: Int) -> Output? {
        let r = Int((windowM / mpp).rounded())
        guard r > 8, w > 2 * r, h > 2 * r else { return nil }
        let bx = w / 2 - r, by = h / 2 - r
        let bw = 2 * r, bh = 2 * r

        // 1. 二值化
        var mask = [UInt8](repeating: 0, count: bw * bh)
        var roadPixels = 0
        for j in 0..<bh {
            let src = (by + j) * w + bx
            let dst = j * bw
            for i in 0..<bw where idx[src + i] == MapGeo.PX_ROAD {
                mask[dst + i] = 1
                roadPixels += 1
            }
        }
        // 道路像素太少说明这块底图还没准备完整，宁可不发也不画一张假路网。
        guard roadPixels > bw * bh / 100 else { return nil }

        // 2. 距离变换（道路宽度 → 主干/次干/支路分级）
        let dist = chamfer(mask, bw, bh)

        // 3. 降采样（块内有路即路，保证窄支路不被抽断）
        let sw = (bw + step - 1) / step, sh = (bh + step - 1) / step
        var small = [UInt8](repeating: 0, count: sw * sh)
        for j in 0..<bh {
            let src = j * bw, sy = j / step
            for i in 0..<bw where mask[src + i] == 1 { small[sy * sw + i / step] = 1 }
        }

        // 4. Zhang-Suen 细化成 1 px 骨架
        let skel = thin(small, sw, sh)

        // 5. 骨架追踪成折线
        let raw = trace(skel, sw, sh, minPts: 4)
        guard !raw.isEmpty else { return nil }

        // 6. 转平面米坐标 + 量宽度
        let px = Double(step) * mpp                       // 降采样后每像素米数
        let x0 = originX + Double(bx) * mpp
        let y0 = originY - Double(by) * mpp
        var lines: [RoadLine] = []
        lines.reserveCapacity(raw.count)
        for path in raw {
            var pts: [Double] = []
            pts.reserveCapacity(path.count * 2)
            var wsum = 0.0, wn = 0
            var lastX = Int.min, lastY = Int.min
            for k in path {
                let sx = k % sw, sy = k / sw
                if sx == lastX && sy == lastY { continue }
                lastX = sx; lastY = sy
                pts.append(x0 + Double(sx) * px)
                pts.append(y0 - Double(sy) * px)
                // 原分辨率上的距离变换值 × 2 = 道路宽度（米）；dist 是窗口局部坐标，不能拿原图坐标索引
                let mx = sx * step, my = sy * step
                if mx < bw && my < bh {
                    wsum += Double(dist[my * bw + mx]); wn += 1
                }
            }
            guard pts.count >= 6 else { continue }
            let width = wn > 0 ? (wsum / Double(wn)) * 2.0 * mpp : 0
            lines.append(RoadLine(pts: simplify(pts, tol: 3.5), width: width))
        }
        guard lines.count >= 3 else { return nil }

        // 7. 按分位数分级：真实底图里没有道路等级，用宽度自适应分出三层，
        //    比写死绝对阈值稳（不同城市、不同缩放宽度差很多）。
        let widths = lines.map { $0.width }.sorted()
        let mid = widths[widths.count / 2]
        let hiCut = mid * 1.25, loCut = mid * 0.82
        var feats: [MapGeo.Feature] = []
        feats.reserveCapacity(lines.count)
        var points = 0
        for l in lines {
            // 标准密度：丢掉最细的一档（支路），只留主干与次干
            let cls: Int
            if l.width >= hiCut { cls = 1 }
            else if l.width >= loCut { cls = 2 }
            else { continue }
            feats.append(MapGeo.Feature(cls: cls, poly: false, pts: l.pts))
            points += l.pts.count / 2
        }
        // 极端情况（整张图只有一种宽度）会让分级塌成一档，此时退回全都要。
        if feats.count < 3 {
            feats = lines.map { MapGeo.Feature(cls: $0.width >= mid ? 1 : 2, poly: false, pts: $0.pts) }
            points = feats.reduce(0) { $0 + $1.pts.count / 2 }
        }

        let cx = originX + Double(w) * mpp / 2
        let cy = originY - Double(h) * mpp / 2
        let data = MapGeo.encodeChunk(frameId: frameId, cx: cx, cy: cy, half: windowM,
                                      feats: feats, maxBytes: maxBytes, maxFeat: maxFeat)
        guard data.count > 24 else { return nil }
        return Output(data: data, lines: feats.count, points: points)
    }

    // MARK: - 距离变换（chamfer 3-4，只用来量宽度，不需要精确欧氏距离）

    private static func chamfer(_ mask: [UInt8], _ w: Int, _ h: Int) -> [Float] {
        let INF: Float = 1e6
        var d = [Float](repeating: 0, count: w * h)
        for i in 0..<(w * h) where mask[i] != 0 { d[i] = INF }
        for y in 0..<h {
            let row = y * w
            for x in 0..<w {
                let k = row + x
                if d[k] == 0 { continue }
                var best = d[k]
                if x > 0 { best = min(best, d[k - 1] + 1) }
                if y > 0 {
                    best = min(best, d[k - w] + 1)
                    if x > 0 { best = min(best, d[k - w - 1] + 1.414) }
                    if x < w - 1 { best = min(best, d[k - w + 1] + 1.414) }
                }
                d[k] = best
            }
        }
        for y in stride(from: h - 1, through: 0, by: -1) {
            let row = y * w
            for x in stride(from: w - 1, through: 0, by: -1) {
                let k = row + x
                if d[k] == 0 { continue }
                var best = d[k]
                if x < w - 1 { best = min(best, d[k + 1] + 1) }
                if y < h - 1 {
                    best = min(best, d[k + w] + 1)
                    if x < w - 1 { best = min(best, d[k + w + 1] + 1.414) }
                    if x > 0 { best = min(best, d[k + w - 1] + 1.414) }
                }
                d[k] = best
            }
        }
        return d
    }

    // MARK: - Zhang-Suen 细化

    private static func thin(_ src: [UInt8], _ w: Int, _ h: Int) -> [UInt8] {
        var img = src
        guard w > 2, h > 2 else { return img }
        var round = 0
        while round < 24 {
            round += 1
            var removed = 0
            for phase in 0..<2 {
                var kill: [Int] = []
                kill.reserveCapacity(512)
                img.withUnsafeBufferPointer { p in
                    for y in 1..<(h - 1) {
                        let row = y * w
                        for x in 1..<(w - 1) {
                            let k = row + x
                            if p[k] == 0 { continue }
                            let p2 = p[k - w], p3 = p[k - w + 1], p4 = p[k + 1], p5 = p[k + w + 1]
                            let p6 = p[k + w], p7 = p[k + w - 1], p8 = p[k - 1], p9 = p[k - w - 1]
                            let nb = Int(p2) + Int(p3) + Int(p4) + Int(p5)
                                   + Int(p6) + Int(p7) + Int(p8) + Int(p9)
                            if nb < 2 || nb > 6 { continue }
                            var trans = 0
                            if p2 == 0 && p3 == 1 { trans += 1 }
                            if p3 == 0 && p4 == 1 { trans += 1 }
                            if p4 == 0 && p5 == 1 { trans += 1 }
                            if p5 == 0 && p6 == 1 { trans += 1 }
                            if p6 == 0 && p7 == 1 { trans += 1 }
                            if p7 == 0 && p8 == 1 { trans += 1 }
                            if p8 == 0 && p9 == 1 { trans += 1 }
                            if p9 == 0 && p2 == 1 { trans += 1 }
                            if trans != 1 { continue }
                            if phase == 0 {
                                if p2 * p4 * p6 != 0 || p4 * p6 * p8 != 0 { continue }
                            } else {
                                if p2 * p4 * p8 != 0 || p2 * p6 * p8 != 0 { continue }
                            }
                            kill.append(k)
                        }
                    }
                }
                if kill.isEmpty { continue }
                removed += kill.count
                for k in kill { img[k] = 0 }
            }
            if removed == 0 { break }
        }
        return img
    }

    // MARK: - 骨架追踪（8 邻接，交叉点处断开成独立折线）

    private static let nb8: [(Int, Int)] = [(-1,-1),(0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0)]

    private static func neighbours(_ k: Int, _ w: Int, _ h: Int, _ pts: Set<Int>, _ out: inout [Int]) {
        out.removeAll(keepingCapacity: true)
        let x = k % w, y = k / w
        for (dx, dy) in nb8 {
            let nx = x + dx, ny = y + dy
            if nx < 0 || ny < 0 || nx >= w || ny >= h { continue }
            let q = ny * w + nx
            if pts.contains(q) { out.append(q) }
        }
    }

    private static func edgeKey(_ a: Int, _ b: Int) -> Int {
        a < b ? a * 131_072 + b : b * 131_072 + a
    }

    private static func trace(_ skel: [UInt8], _ w: Int, _ h: Int, minPts: Int) -> [[Int]] {
        var pts = Set<Int>()
        pts.reserveCapacity(4096)
        for k in 0..<(w * h) where skel[k] != 0 { pts.insert(k) }
        guard pts.count > 8 else { return [] }

        var shelf = [Int]()
        var degree = [Int: Int]()
        degree.reserveCapacity(pts.count)
        for k in pts {
            neighbours(k, w, h, pts, &shelf)
            degree[k] = shelf.count
        }
        // 端点（1 邻居）和交叉点（>=3 邻居）是折线的起止
        let nodes = Set(degree.filter { $0.value != 2 }.keys)
        var used = Set<Int>()
        used.reserveCapacity(pts.count * 2)
        var lines: [[Int]] = []
        var path = [Int]()

        for s in nodes {
            neighbours(s, w, h, pts, &shelf)
            let seeds = shelf                     // 拷贝一份：循环体里会复用 shelf
            for f in seeds {
                let e0 = edgeKey(s, f)
                if used.contains(e0) { continue }
                path.removeAll(keepingCapacity: true)
                path.append(s); path.append(f)
                used.insert(e0)
                var prev = s, cur = f
                while path.count < 4096 {
                    if nodes.contains(cur) { break }
                    neighbours(cur, w, h, pts, &shelf)
                    var nextQ = -1
                    for q in shelf where q != prev {
                        if used.contains(edgeKey(cur, q)) { continue }
                        // 防止在 2x2 块里原地打转
                        if path.count >= 3 && path[path.count - 2] == q { continue }
                        nextQ = q; break
                    }
                    if nextQ < 0 { break }
                    used.insert(edgeKey(cur, nextQ))
                    path.append(nextQ)
                    prev = cur; cur = nextQ
                }
                if path.count >= minPts { lines.append(path) }
            }
        }

        // 没有端点也没有交叉点的纯环：兜底成一条闭合折线
        var covered = Set<Int>()
        for l in lines { covered.formUnion(l) }
        var rest = pts.subtracting(covered)
        while !rest.isEmpty {
            guard let s = rest.first else { break }
            path.removeAll(keepingCapacity: true)
            path.append(s)
            var prev = -1, cur = s
            while path.count < 4096 {
                neighbours(cur, w, h, pts, &shelf)
                var nextQ = -1
                for q in shelf where q != prev && !path.contains(q) { nextQ = q; break }
                if nextQ < 0 { break }
                path.append(nextQ); prev = cur; cur = nextQ
                if nextQ == s { break }
            }
            rest.subtract(path)
            if path.count >= minPts { lines.append(path) }
        }
        return lines
    }

    // MARK: - 简化（骨架是 4 连通台阶，先把台阶压平再交给打包器）

    /// 道格拉斯-普克。骨架每条线最多几百点，用显式栈避免递归爆栈。
    private static func simplify(_ pts: [Double], tol: Double) -> [Double] {
        let n = pts.count / 2
        guard n > 2 else { return pts }
        var keep = [Bool](repeating: false, count: n)
        keep[0] = true; keep[n - 1] = true
        var stack: [(Int, Int)] = [(0, n - 1)]
        while let range = stack.popLast() {
            let a = range.0, b = range.1
            if b - a < 2 { continue }
            let ax = pts[2 * a], ay = pts[2 * a + 1]
            let bx = pts[2 * b], by = pts[2 * b + 1]
            let dx = bx - ax, dy = by - ay
            let len = (dx * dx + dy * dy).squareRoot()
            var bestD = -1.0, bestI = -1
            for i in (a + 1)..<b {
                let px = pts[2 * i], py = pts[2 * i + 1]
                let d: Double
                if len < 1e-9 { d = ((px - ax) * (px - ax) + (py - ay) * (py - ay)).squareRoot() }
                else { d = abs(dy * (px - ax) - dx * (py - ay)) / len }
                if d > bestD { bestD = d; bestI = i }
            }
            if bestD > tol, bestI > 0 {
                keep[bestI] = true
                stack.append((a, bestI))
                stack.append((bestI, b))
            }
        }
        var out: [Double] = []
        out.reserveCapacity(n * 2)
        for i in 0..<n where keep[i] {
            out.append(pts[2 * i]); out.append(pts[2 * i + 1])
        }
        return out
    }
}
