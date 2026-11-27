import SwiftUI
import UIKit
import CoreLocation
import ObjectiveC

/// A separate north-up map renderer. It never changes the route preview/navigation camera.
final class AmapCaptureHost: NSObject {
    static let shared = AmapCaptureHost()
    struct Shot {
        let pixels: [UInt32], withoutBuildings: [UInt32]?, width: Int, height: Int
        let left: Double, top: Double, right: Double, bottom: Double
        let north: Double, south: Double
    }
    private weak var container: UIView?
    private var map: MAMapView?
    private var serial = 0
    private var pending: ((Result<Shot, Error>) -> Void)?
    private var started = Date.distantPast
    private var reference: [UInt32]?
    private var referenceProjection: [Double]?
    private var emptyBuildingChecks = 0
    private var timer: Timer?
    private var observer: NSObjectProtocol?
    private var satelliteCapture=false
    private var mpp: Double { satelliteCapture ? 3.5 : 2.0 }

    func attach(_ view: UIView) {
        container = view
        #if DEBUG
        MapCaptureCheck.startIfRequested()
        #endif
        if observer == nil {
            observer = NotificationCenter.default.addObserver(forName: UIApplication.willResignActiveNotification, object: nil, queue: .main) { [weak self] _ in self?.cancel() }
        }
    }
    func detach(_ view: UIView) {
        if container === view { cancel(); map?.removeFromSuperview(); map = nil; container = nil }
    }
    private func ensureMap(size: CGFloat) throws -> MAMapView {
        guard UIApplication.shared.applicationState == .active, let view = container, view.window != nil else {
            throw MapProcessingError.invalid("高德截图需保持 App 在前台；后台使用已离线的地图")
        }
        guard !Settings.shared.amapKeyInUse.isEmpty else { throw MapProcessingError.invalid("请先填写高德 iOS Key") }
        let m: MAMapView
        if let existing = map { m = existing } else {
            m = MAMapView(frame: CGRect(x: 0, y: 0, width: size, height: size))
            m.contentScaleFactor = 1; m.layer.contentsScale = 1
            m.mapType = .standard
            m.isShowsLabels = false; m.isShowsBuildings = false; m.isShowsIndoorMap = false
            m.showsUserLocation = false; m.isShowTraffic = false; m.showsCompass = false; m.showsScale = false
            m.isZoomEnabled = false; m.isScrollEnabled = false; m.isRotateEnabled = false; m.isRotateCameraEnabled = false
            m.isAccessibilityElement = false; m.accessibilityElementsHidden = true
            class_addProtocol(AmapCaptureHost.self, MAMapViewDelegate.self)
            m.delegate = (self as AnyObject) as? MAMapViewDelegate
            view.addSubview(m); map = m
        }
        m.transform = .identity
        m.bounds = CGRect(x: 0, y: 0, width: size, height: size)
        m.center = CGPoint(x: size / 2, y: size / 2)
        // GLKit uses the displayed geometry for its drawable. A one-point transform produces
        // transparent snapshots even when the SDK reports state=1. Keep actual render geometry.
        m.layoutIfNeeded()
        m.renderringDisabled = false
        m.isAllowDecreaseFrame = false
        #if DEBUG
        if ProcessInfo.processInfo.arguments.contains("--map-capture-check"),
           !ProcessInfo.processInfo.arguments.contains("--map-capture-hidden"), let window = view.window {
            window.addSubview(m)
            m.frame = CGRect(x: 0, y: 0, width: size, height: size)
        }
        #endif
        return m
    }
    @objc(mapViewDidFailLoadingMap:withError:)
    func mapLoadFailed(_ map: MAMapView, error: NSError) {
        finish(.failure(MapProcessingError.invalid("高德地图加载失败：\(error.domain) \(error.code)")))
    }
    func capture(_ frame: MapGeo.Frame, x: Int, y: Int, size n: Int, satellite: Bool = false, style: Int = 1, completion: @escaping (Result<Shot, Error>) -> Void) {
        precondition(Thread.isMainThread)
        guard pending == nil else { completion(.failure(MapProcessingError.invalid("高德截图正在处理上一块"))); return }
        do {
            let m = try ensureMap(size: CGFloat(AmapCapturePolicy.renderSize(for: n)))
            satelliteCapture=satellite
            m.mapType = satellite ? .satellite : .standard
            m.isShowsLabels = style == 0
            pending = completion; reference = nil; referenceProjection = nil; emptyBuildingChecks = 0; started = Date(); serial += 1
            let id = serial
            m.isShowsBuildings = false; m.rotationDegree = 0; m.cameraDegree = n == 600 ? 0.05 : 0
            let margin = n == 600 ? 20.0 : 100.0
            let north = frame.lat(Double(y) + margin), south = frame.lat(Double(y) - Double(n)*self.mpp - margin)
            let west = frame.lng(Double(x) - margin), east = frame.lng(Double(x) + Double(n)*self.mpp + margin)
            let nw = MAMapPointForCoordinate(CLLocationCoordinate2D(latitude: north, longitude: west))
            let se = MAMapPointForCoordinate(CLLocationCoordinate2D(latitude: south, longitude: east))
            let rect = MAMapRectMake(min(nw.x, se.x), min(nw.y, se.y), abs(se.x - nw.x), abs(se.y - nw.y))
            // The SDK's internal viewport catches up on a render tick after UIView resizing.
            // Fitting immediately uses the old 720-point viewport and drops building zoom below 17.
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.35) {
                guard self.serial == id, self.pending != nil else { return }
                m.setVisibleMapRect(rect, animated: false)
                m.cameraDegree = n == 600 ? 0.05 : 0
                DispatchQueue.main.asyncAfter(deadline: .now() + 1) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }
            }
            timer = Timer.scheduledTimer(withTimeInterval: n == 600 ? 24 : 40, repeats: false) { [weak self] _ in
                guard let self = self, self.serial == id else { return }
                self.finish(.failure(MapProcessingError.invalid("高德取图超时，保留已有地图")))
            }
        } catch { completion(.failure(error)) }
    }
    private func snapshot(_ id: Int, frame: MapGeo.Frame, x: Int, y: Int, n: Int) {
        guard serial == id, pending != nil, let m = map else { return }
        guard UIApplication.shared.applicationState == .active else { cancel(); return }
        m.takeSnapshot(in: m.bounds, timeoutInterval: 3) { [weak self] image, state in
            DispatchQueue.main.async {
                guard let self = self, self.serial == id, self.pending != nil else { return }
                guard state == 1, let cg = image?.cgImage else {
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }; return
                }
                #if DEBUG
                MapCaptureCheck.record(image: image, map: m, tile: n, buildings: self.reference != nil)
                #endif
                do {
                    let width = min(cg.width, 2880), height = min(cg.height, 2880)
                    guard width <= 5000, height <= 5000, width > 0, height > 0,
                          abs(m.rotationDegree) < 0.1, abs(m.cameraDegree) < 0.1 else { throw MapProcessingError.invalid("高德截图尺寸或方向异常") }
                    if n == 600, m.zoomLevel < 17 { throw MapProcessingError.invalid("建筑取图缩放级别不足") }
                    let north = frame.lat(Double(y)), south = frame.lat(Double(y) - Double(n)*self.mpp)
                    let tl = m.convert(CLLocationCoordinate2D(latitude: north, longitude: frame.lng(Double(x))), toPointTo: m)
                    let tr = m.convert(CLLocationCoordinate2D(latitude: north, longitude: frame.lng(Double(x) + Double(n)*self.mpp)), toPointTo: m)
                    let bl = m.convert(CLLocationCoordinate2D(latitude: south, longitude: frame.lng(Double(x))), toPointTo: m)
                    let br = m.convert(CLLocationCoordinate2D(latitude: south, longitude: frame.lng(Double(x) + Double(n)*self.mpp)), toPointTo: m)
                    let sx = Double(width) / Double(m.bounds.width), sy = Double(height) / Double(m.bounds.height)
                    guard abs(tl.y - tr.y) * sy <= 2, abs(tl.x - bl.x) * sx <= 2,
                          abs(bl.y - br.y) * sy <= 2, abs(tr.x - br.x) * sx <= 2,
                          Double(tr.x - tl.x) * sx >= Double(n - 2), Double(bl.y - tl.y) * sy >= Double(n - 2) else { throw MapProcessingError.invalid("高德截图投影未对齐或精度不足") }
                    var pixels = [UInt32](repeating: 0, count: width * height)
                    let ok = pixels.withUnsafeMutableBytes { raw -> Bool in
                        guard let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue) else { return false }
                        context.draw(cg, in: CGRect(x: 0, y: 0, width: width, height: height)); return true
                    }
                    guard ok else { throw MapProcessingError.invalid("高德截图像素解码失败") }
                    // SDK state=1 can precede the first GPU frame. Do not publish a transparent image.
                    if stride(from: 0, to: pixels.count, by: 97).contains(where: { pixels[$0] >> 24 < 250 }) {
                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }; return
                    }
                    let projection = [Double(tl.x) * sx, Double(tl.y) * sy, Double(tr.x) * sx, Double(bl.y) * sy]
                    // Both shots share exactly the same camera. Only the SDK building layer changes.
                    if n == 600, self.reference == nil {
                        self.reference = pixels; self.referenceProjection = projection; m.isShowsBuildings = true
                        DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }; return
                    }
                    if n == 600, let reference = self.reference {
                        guard reference.count == pixels.count, let original = self.referenceProjection, AmapCapturePolicy.aligned(original, projection) else { throw MapProcessingError.invalid("建筑两次截图尺寸或相机发生变化") }
                        let changed = zip(reference, pixels).contains { pair in
                            AmapBuildings.colorDifference(pair.0, pair.1) >= 4
                        }
                        if !changed && self.emptyBuildingChecks < 2 {
                            self.emptyBuildingChecks += 1
                            DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }; return
                        }
                    }
                    self.finish(.success(Shot(pixels: pixels, withoutBuildings: self.reference, width: width, height: height,
                                             left: Double(tl.x) * sx, top: Double(tl.y) * sy, right: Double(tr.x) * sx, bottom: Double(bl.y) * sy, north: north, south: south)))
                } catch { self.finish(.failure(error)) }
            }
        }
    }
    private func finish(_ result: Result<Shot, Error>) {
        guard let done = pending else { return }
        pending = nil; serial += 1; reference = nil; referenceProjection = nil; timer?.invalidate(); timer = nil
        if UIApplication.shared.applicationState == .active { map?.isShowsBuildings = false }
        map?.renderringDisabled = true; map?.isAllowDecreaseFrame = true
        #if DEBUG
        if ProcessInfo.processInfo.arguments.contains("--map-capture-check"), let m = map, let view = container { view.addSubview(m) }
        #endif
        done(result)
    }
    func cancel() {
        if perspPending != nil { finishPersp(.failure(MapProcessingError.cancelled)) }
        finish(.failure(MapProcessingError.cancelled))
    }

    // MARK: - 原生 3D 底图（P3D，bulk type 13）
    //
    // 让高德按俯仰角直接渲染一帧带透视的鸟瞰图（楼块、侧面、立交桥都由高德画），
    // 同时把「地面距离 → 画布行 / 每列多少毫米」这张查找表一起算好。
    // 固件每屏行查一次表就能把画布贴到屏幕上（不再做楼块挤出、逐像素透视除法）。
    //
    // 相机中心比车后移 `back` 米：固件里车后那 1/4 屏（y > t3dPy）距离是负的，
    // 只有画布覆盖到车后，底部才不会被判成「太近」而涂成地面色。
    struct PerspShot {
        let pixels: [UInt32]
        let width: Int, height: Int
        let bearing: Double, pitch: Double
        let dMin: Int, dMax: Int
        let lut: [Persp3DPacket.LUTPoint]
        let zoom: Double
    }
    private struct PerspJob {
        let id: Int
        let frame: MapGeo.Frame
        let cx: Double, cy: Double                 // 相机中心世界坐标（米）
        let anchorX: Double, anchorY: Double       // 固件用的锚点（车的位置，取整）
        let bearing: Double, pitch: Double, zoom: Double
        let back: Double
        let size: Int
        let lutN: Int
        var flipped: Bool
    }
    /// 高德 rotationDegree 的正负约定：0 表示还没标定，-1 = rotationDegree 给 -bearing，+1 = 给 bearing。
    private static var perspRotationSign = 0
    private var perspJob: PerspJob?
    private var perspPending: ((Result<PerspShot, Error>) -> Void)?

    /// 取一帧透视底图 + LUT。`ax/ay` 是世界坐标（米，和 `#p` 同一坐标系），`bearing` 是行进朝向。
    func capturePersp(frame f: MapGeo.Frame, ax: Double, ay: Double, bearing: Double, pitch: Double,
                      back: Double = 90, size n: Int = 448, lutN: Int = 64,
                      completion: @escaping (Result<PerspShot, Error>) -> Void) {
        precondition(Thread.isMainThread)
        guard pending == nil, perspPending == nil else {
            completion(.failure(MapProcessingError.invalid("高德正在取上一块地图")))
            return
        }
        do {
            _ = try ensureMap(size: CGFloat(n))
            guard let m = map else { throw MapProcessingError.invalid("高德底图未创建") }
            satelliteCapture = false
            m.mapType = .standard
            m.isShowsLabels = false
            m.isShowsIndoorMap = false
            m.showsUserLocation = false
            m.isShowTraffic = false
            serial += 1
            let axr = ax.rounded(), ayr = ay.rounded()
            let rad = bearing * Double.pi / 180
            let cx = axr - sin(rad) * back, cy = ayr - cos(rad) * back
            let job = PerspJob(id: serial, frame: f, cx: cx, cy: cy, anchorX: axr, anchorY: ayr,
                               bearing: bearing, pitch: max(15, min(55, pitch)), zoom: 17.2,
                               back: back, size: n, lutN: max(8, min(64, lutN)), flipped: false)
            perspPending = completion
            perspJob = job
            started = Date()
            timer?.invalidate()
            timer = Timer.scheduledTimer(withTimeInterval: 22, repeats: false) { [weak self] _ in
                guard let self = self, self.perspJob?.id == job.id else { return }
                self.finishPersp(.failure(MapProcessingError.invalid("高德 3D 取图超时，保留已有底图")))
            }
            applyPerspCamera(job)
        } catch {
            perspPending = nil; perspJob = nil
            completion(.failure(error))
        }
    }

    private static func perspRotation(for bearing: Double) -> CGFloat {
        let sign = perspRotationSign == 0 ? -1 : perspRotationSign
        var r = (sign > 0 ? bearing : -bearing).truncatingRemainder(dividingBy: 360)
        if r < 0 { r += 360 }
        return CGFloat(r)
    }

    private func applyPerspCamera(_ job: PerspJob) {
        guard perspJob?.id == job.id, let m = map else { return }
        m.isShowsBuildings = true
        m.centerCoordinate = CLLocationCoordinate2D(latitude: job.frame.lat(job.cy), longitude: job.frame.lng(job.cx))
        m.setZoomLevel(CGFloat(job.zoom), animated: false)
        m.cameraDegree = CGFloat(job.pitch)
        m.rotationDegree = Self.perspRotation(for: job.bearing)
        // SDK 的相机会在一次渲染节拍后才真正生效，投影查询要等这一拍。
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.55) { [weak self] in self?.verifyPersp(job) }
    }

    /// 投影查询某个「相机前方多少米 / 右侧多少米」的地面点。z 不参与，只用平面坐标。
    private func perspPoint(_ job: PerspJob, camDistance: Double, lateral: Double) -> CGPoint? {
        guard let m = map else { return nil }
        let rad = job.bearing * Double.pi / 180
        let north = camDistance * cos(rad) - lateral * sin(rad)
        let east = camDistance * sin(rad) + lateral * cos(rad)
        let coord = CLLocationCoordinate2D(latitude: job.frame.lat(job.cy + north), longitude: job.frame.lng(job.cx + east))
        return m.convert(coord, toPointTo: m)
    }

    private func verifyPersp(_ job: PerspJob) {
        guard perspJob?.id == job.id, let m = map else { return }
        m.cameraDegree = CGFloat(job.pitch)
        m.rotationDegree = Self.perspRotation(for: job.bearing)
        let mid = CGPoint(x: m.bounds.midX, y: m.bounds.midY)
        guard let ahead = perspPoint(job, camDistance: 120, lateral: 0),
              let right = perspPoint(job, camDistance: 60, lateral: 40) else {
            finishPersp(.failure(MapProcessingError.invalid("高德 3D 投影不可用")))
            return
        }
        let up = ahead.y < mid.y - 4          // 前方在地平线（屏幕上方）
        let east = right.x > mid.x + 4        // 右侧在右
        if up && east {
            if Self.perspRotationSign == 0 { Self.perspRotationSign = -1 }
            measurePersp(job)
            return
        }
        if !job.flipped {
            // 朝向约定反了：翻一次再验，成功后记住这个符号，后面每帧不再试错。
            Self.perspRotationSign = Self.perspRotationSign == 1 ? -1 : 1
            var flipped = job
            flipped.flipped = true
            perspJob = flipped
            m.rotationDegree = Self.perspRotation(for: job.bearing)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.45) { [weak self] in self?.verifyPersp(flipped) }
            return
        }
        finishPersp(.failure(MapProcessingError.invalid("高德 3D 朝向标定失败")))
    }

    /// 用投影反解「画布顶/底行对应多少米」，再按对数均分算 LUT。全程只做投影查询，不重新渲染。
    private func measurePersp(_ job: PerspJob) {
        guard perspJob?.id == job.id, let m = map else { return }
        let h = Double(m.bounds.height), w = Double(m.bounds.width)
        guard h > 8, w > 8 else { finishPersp(.failure(MapProcessingError.invalid("高德 3D 画布尺寸异常"))); return }

        func rowY(_ camDistance: Double) -> Double {
            guard let p = perspPoint(job, camDistance: camDistance, lateral: 0) else { return h }
            return Double(p.y)
        }
        // 顶行（远处）：y 随距离单调减小
        var lo = 0.0, hi = 6000.0
        for _ in 0..<26 {
            let mid = (lo + hi) / 2
            if rowY(mid) > 0.5 { lo = mid } else { hi = mid }
        }
        let dTopCam = (lo + hi) / 2
        // 底行（近处，可能落到相机后方，所以从 0 往负方向找）
        var blo = -900.0, bhi = 0.0
        for _ in 0..<26 {
            let mid = (blo + bhi) / 2
            if rowY(mid) < h - 0.5 { blo = mid } else { bhi = mid }
        }
        let dBotCam = (blo + bhi) / 2

        var dMin = Int(max(2, (job.back + dBotCam).rounded()))
        var dMax = Int(min(65000, (job.back + dTopCam).rounded()))
        if dMax < dMin * 3 { dMax = dMin * 3 }
        guard dMax > dMin * 2 else {
            finishPersp(.failure(MapProcessingError.invalid("高德 3D 视野范围过小")))
            return
        }
        let logMin = log(Double(dMin)), logMax = log(Double(dMax))
        var lut: [Persp3DPacket.LUTPoint] = []
        lut.reserveCapacity(job.lutN)
        var lastMM = 0.0
        for k in 0..<job.lutN {
            let t = job.lutN > 1 ? Double(k) / Double(job.lutN - 1) : 0
            let dA = exp(logMin + (logMax - logMin) * t)          // 距锚点（车）的米数
            let dc = dA - job.back                                // 距相机中心的米数
            let v = min(max(rowY(dc), 0), h - 1)
            var mm = lastMM
            if let l = perspPoint(job, camDistance: dc, lateral: -20),
               let r = perspPoint(job, camDistance: dc, lateral: 20) {
                let span = abs(Double(r.x - l.x))
                if span > 1.2 { mm = 40000.0 / span }             // 40 米宽 / 跨了多少画布列
            }
            if mm <= 0 { mm = lastMM > 0 ? lastMM : 20000 }
            lastMM = mm
            lut.append(Persp3DPacket.LUTPoint(v: v, mm: mm))
        }
        // 画布像素与视图点可能不同（contentScaleFactor 一般已设 1，这里仍做一次换算）
        let sx = Double(job.size) / w, sy = Double(job.size) / h
        let scaled = lut.map { Persp3DPacket.LUTPoint(v: $0.v * sy, mm: $0.mm / max(0.01, sx)) }
        shotPersp(job, lut: scaled, dMin: dMin, dMax: dMax)
    }

    private func shotPersp(_ job: PerspJob, lut: [Persp3DPacket.LUTPoint], dMin: Int, dMax: Int) {
        guard perspJob?.id == job.id, let m = map else { return }
        guard UIApplication.shared.applicationState == .active else { cancel(); return }
        m.takeSnapshot(in: m.bounds, timeoutInterval: 3) { [weak self] image, state in
            DispatchQueue.main.async {
                guard let self = self, self.perspJob?.id == job.id, self.perspPending != nil else { return }
                guard state == 1, let cg = image?.cgImage, let decoded = Self.decodePersp(cg) else {
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.7) { [weak self] in self?.shotPersp(job, lut: lut, dMin: dMin, dMax: dMax) }
                    return
                }
                if stride(from: 0, to: decoded.pixels.count, by: 97).contains(where: { decoded.pixels[$0] >> 24 < 250 }) {
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.7) { [weak self] in self?.shotPersp(job, lut: lut, dMin: dMin, dMax: dMax) }
                    return
                }
                let sx = Double(decoded.width) / Double(m.bounds.width)
                let sy = Double(decoded.height) / Double(m.bounds.height)
                let fixed = lut.map { Persp3DPacket.LUTPoint(v: $0.v * sy, mm: $0.mm / max(0.01, sx)) }
                self.finishPersp(.success(PerspShot(pixels: decoded.pixels, width: decoded.width, height: decoded.height,
                                                    bearing: job.bearing, pitch: job.pitch, dMin: dMin, dMax: dMax,
                                                    lut: fixed, zoom: Double(m.zoomLevel))))
            }
        }
    }

    /// P3D 单边上限 1024（固件 P3D_MAX_W/H）。takeSnapshot 回来的是 448pt × 3x = 1344px，
    /// 不缩就会被 Persp3DPacket.encode 直接丢掉，界面只留一句「3D 底图打包失败」。
    /// 640 已经比屏幕（466px）宽，再大只是多花几秒蓝牙。
    static let perspMaxSide = 640

    private static func decodePersp(_ cg: CGImage) -> (pixels: [UInt32], width: Int, height: Int)? {
        let longSide = max(cg.width, cg.height)
        let k = longSide > perspMaxSide ? Double(perspMaxSide) / Double(longSide) : 1
        let width = max(16, Int((Double(cg.width) * k).rounded()))
        let height = max(16, Int((Double(cg.height) * k).rounded()))
        guard width > 8, height > 8 else { return nil }
        var pixels = [UInt32](repeating: 0, count: width * height)
        let ok = pixels.withUnsafeMutableBytes { raw -> Bool in
            guard let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8,
                                          bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                                          bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue) else { return false }
            context.draw(cg, in: CGRect(x: 0, y: 0, width: width, height: height))
            return true
        }
        return ok ? (pixels, width, height) : nil
    }

    private func finishPersp(_ result: Result<PerspShot, Error>) {
        guard let done = perspPending else { return }
        perspPending = nil; perspJob = nil; serial += 1
        timer?.invalidate(); timer = nil
        map?.isShowsBuildings = false
        map?.renderringDisabled = true; map?.isAllowDecreaseFrame = true
        done(result)
    }
}

#if DEBUG
/// Explicit developer launch only; captures SDK output without changing navigation or saved settings.
private enum MapCaptureCheck {
    private static var started = false
    private static var enabled: Bool { ProcessInfo.processInfo.arguments.contains("--map-capture-check") }
    private static let folder = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("map-capture-check")
    static func record(image: UIImage?, map: MAMapView, tile: Int, buildings: Bool) {
        guard enabled else { return }
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let name = tile == 1200 ? "base" : (buildings ? "buildings-on" : "buildings-off")
        let info: [String: Any] = ["zoom": map.zoomLevel, "camera": map.cameraDegree, "bounds": NSCoder.string(for: map.bounds), "frame": NSCoder.string(for: map.frame), "imageWidth": image?.cgImage?.width ?? 0, "imageHeight": image?.cgImage?.height ?? 0]
        if let data = try? JSONSerialization.data(withJSONObject: info, options: [.prettyPrinted, .sortedKeys]) { try? data.write(to: folder.appendingPathComponent(name + ".json"), options: .atomic) }
        if let png = image?.pngData() { try? png.write(to: folder.appendingPathComponent(name + ".png"), options: .atomic) }
    }
    static func startIfRequested() {
        guard enabled, !started else { return }
        let args = ProcessInfo.processInfo.arguments
        guard let index = args.firstIndex(of: "--map-capture-check"), args.count >= index + 6,
              let lat = Double(args[index+1]), let lng = Double(args[index+2]), let id = Int(args[index+3]),
              let cx = Double(args[index+4]), let cy = Double(args[index+5]) else { return }
        started = true
        try? FileManager.default.removeItem(at: folder)
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
            DispatchQueue.global(qos: .userInitiated).async {
                var messages = [String]()
                let result = Result { try AmapSdkRaster.build(MapGeo.Frame(refLat: lat, refLng: lng, id: id), cx: cx, cy: cy, includeBuildings: true, shouldCancel: { false }, onBuildingStatus: { messages.append($0) }).bytes }
                try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
                switch result {
                case .success(let packet):
                    try? packet.write(to: folder.appendingPathComponent("map.bin"), options: .atomic)
                    messages.append("buildingFlag=\(RasterPacket.isBuilding(packet))")
                case .failure(let error): messages.append(error.localizedDescription)
                }
                try? messages.joined(separator: "\n").write(to: folder.appendingPathComponent("result.txt"), atomically: true, encoding: .utf8)
            }
        }
    }
}
#endif

struct AmapCaptureSurface: UIViewRepresentable {
    func makeUIView(context: Context) -> UIView {
        let view = UIView(frame: CGRect(x: 0, y: 0, width: 1, height: 1))
        view.isUserInteractionEnabled = false; view.accessibilityElementsHidden = true
        AmapCaptureHost.shared.attach(view); return view
    }
    func updateUIView(_ uiView: UIView, context: Context) {}
    static func dismantleUIView(_ uiView: UIView, coordinator: ()) { AmapCaptureHost.shared.detach(uiView) }
}
