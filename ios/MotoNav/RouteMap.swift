import SwiftUI
import CoreLocation
import ObjectiveC

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.3 新增）
///
/// 选路线时的小地图预览（高德地图，夜间样式）：
/// 选中的路线粉色粗线、其他路线灰色、导入的高德分享原路线橙色虚线，自动缩放到能看全所有路线。
/// iOS v0.4：目的地红色大头针、途经点紫色大头针。iOS v0.5.2：地图跟着夜间配色切换白天 / 夜间样式。
/// iOS v0.15.32：路线起点蓝色图钉（与终点同视觉尺寸；蓝头 + 白芯 + 白杆）。
/// 放在可以上下滑的主界面里，所以地图本身不能拖动，只用来看。
struct RouteMapView: UIViewRepresentable {
    let routes: [NaviBridge.RouteOption]
    let selected: Int
    let ghost: [CLLocationCoordinate2D]
    var dest: CLLocationCoordinate2D? = nil
    var vias: [CLLocationCoordinate2D] = []
    var night = true                                // iOS v0.5.2 跟着夜间配色：夜间用深色地图，白天用浅色地图

    func makeCoordinator() -> RouteMapDelegate { RouteMapDelegate() }

    func makeUIView(context: Context) -> MAMapView {
        let m = MAMapView(frame: .zero)
        m.mapType = night ? .standardNight : .standard
        // 预览图嵌在 ScrollView 里：关掉定位蓝点动画，减轻上下滑时的 Metal 合成压力
        m.showsUserLocation = false
        m.isZoomEnabled = false
        m.isScrollEnabled = false
        m.isRotateEnabled = false
        m.isRotateCameraEnabled = false
        m.showsCompass = false
        m.showsScale = false
        m.delegate = context.coordinator.asDelegate
        return m
    }

    func updateUIView(_ m: MAMapView, context: Context) {
        let type: MAMapType = night ? .standardNight : .standard
        if m.mapType != type { m.mapType = type }
        let start = routes.first(where: { $0.id == selected })?.coords.first
        // 内容没变就不重画
        let key = routes.map { "\($0.id):\($0.coords.count)" }.joined(separator: ",") + "|\(selected)|\(ghost.count)" +
            "|\(dest.map { "\($0.latitude),\($0.longitude)" } ?? "")|" + vias.map { "\($0.latitude),\($0.longitude)" }.joined(separator: ";") +
            "|\(start.map { "\($0.latitude),\($0.longitude)" } ?? "")"
        if context.coordinator.key == key { return }
        context.coordinator.key = key
        if let old = m.overlays { m.removeOverlays(old) }
        let oldPins = (m.annotations ?? []).compactMap { $0 as? MAPointAnnotation }
        if !oldPins.isEmpty { m.removeAnnotations(oldPins) }
        var pins: [MAPointAnnotation] = []
        if let s = start {
            let a = MAPointAnnotation(); a.coordinate = s; a.title = "起点"; pins.append(a)
        }
        for (i, v) in vias.enumerated() {
            let a = MAPointAnnotation(); a.coordinate = v; a.title = "途经 \(i + 1)"; pins.append(a)
        }
        if let d = dest { let a = MAPointAnnotation(); a.coordinate = d; a.title = "目的地"; pins.append(a) }
        if !pins.isEmpty { m.addAnnotations(pins) }
        var lines: [MAPolyline] = []
        func add(_ coords: [CLLocationCoordinate2D], _ kind: String) {
            if coords.count < 2 { return }
            var c = coords
            if let l = MAPolyline(coordinates: &c, count: UInt(c.count)) { l.title = kind; lines.append(l) }
        }
        add(ghost, "ghost")
        for r in routes where r.id != selected { add(r.coords, "alt") }
        if let s = routes.first(where: { $0.id == selected }) { add(s.coords, "sel") }    // 选中的最后画（在最上面）
        if lines.isEmpty { return }
        m.addOverlays(lines)
        m.showOverlays(lines, edgePadding: UIEdgeInsets(top: 28, left: 28, bottom: 28, right: 28), animated: false)
    }
}

/// 地图回调：决定每条线画成什么样。和 NaviBridge 一样在运行时登记协议（见 CLAUDE.md 高德坑第 1 条）
/// Not `final`: MotorMapCoordinator subclasses for user-zoom / follow callbacks.
class RouteMapDelegate: NSObject {
    var key = ""

    var asDelegate: MAMapViewDelegate? {
        // Register on the dynamic class (subclass or base) so Motormap overrides are visible to ObjC.
        class_addProtocol(type(of: self), MAMapViewDelegate.self)
        return (self as AnyObject) as? MAMapViewDelegate
    }

    /// 大头针：起点蓝 / 目的地红（同一钉视图只换头色；途经点紫色系统钉）
    @objc(mapView:viewForAnnotation:)
    func annotationView(_ m: MAMapView, annotation: MAAnnotation) -> MAAnnotationView? {
        guard let a = annotation as? MAPointAnnotation else { return nil }
        if a.title == "起点" || a.title == "目的地" {
            let id = "endpoint-pin"
            let v = (m.dequeueReusableAnnotationView(withIdentifier: id) as? RouteEndpointPinView)
                ?? RouteEndpointPinView(annotation: a, reuseIdentifier: id)
            v.annotation = a
            // mockup parity：同一 chrome，仅换头色（蓝 `#0A84FF` / 系统红）
            v.apply(headColor: a.title == "起点" ? RouteEndpointPinView.startBlue : RouteEndpointPinView.endRed)
            return v
        }
        let id = "pin"
        let v = (m.dequeueReusableAnnotationView(withIdentifier: id) as? MAPinAnnotationView) ?? MAPinAnnotationView(annotation: a, reuseIdentifier: id)
        v?.annotation = a
        v?.pinColor = .purple
        v?.canShowCallout = false
        return v
    }

    @objc(mapView:rendererForOverlay:)
    func renderer(_ m: MAMapView, overlay: MAOverlay) -> MAOverlayRenderer? {
        guard let pl = overlay as? MAPolyline, let r = MAPolylineRenderer(polyline: pl) else { return nil }
        r.lineJoinType = kMALineJoinRound
        r.lineCapType = kMALineCapRound
        switch pl.title {
        case "sel":
            r.strokeColor = UIColor(red: 1, green: 0.31, blue: 0.71, alpha: 1)
            r.lineWidth = 8
        case "ghost":
            r.strokeColor = UIColor(red: 1, green: 0.62, blue: 0.15, alpha: 0.95)
            r.lineWidth = 4
            r.lineDashType = kMALineDashTypeSquare
        default:
            r.strokeColor = UIColor(white: 0.6, alpha: 0.7)
            r.lineWidth = 6
        }
        return r
    }
}

/// 路线预览起终点钉：同一视图 / 同一尺寸，只换头色。
/// 尺寸对齐 mockup parity（@1260×2736，按 3× 屏换算 pt）：
/// 头径 41px、杆宽 5px、杆长 ~71px（≈1.7×头径）；左上白高光 + 尖端触点阴影。
final class RouteEndpointPinView: MAAnnotationView {
    /// SF Blue `#0A84FF`
    static let startBlue = UIColor(red: 0.039, green: 0.518, blue: 1.0, alpha: 1)
    /// 系统红（深色 `#FF453A`）
    static let endRed = UIColor(red: 1.0, green: 0.271, blue: 0.227, alpha: 1)

    // mockup px ÷ 3（1260 宽 ≈ 3× 逻辑宽）
    private static let head: CGFloat = 41.0 / 3.0          // ≈13.67
    private static let stemWidth: CGFloat = 5.0 / 3.0      // ≈1.67
    private static let stemLength: CGFloat = 71.0 / 3.0    // ≈23.67
    private static let tipDot: CGFloat = 4.0 / 3.0
    private static let canvas = CGSize(
        width: head + 4,
        height: head + stemLength + tipDot
    )

    override init(annotation: MAAnnotation?, reuseIdentifier: String?) {
        super.init(annotation: annotation, reuseIdentifier: reuseIdentifier)
        bounds = CGRect(origin: .zero, size: Self.canvas)
        centerOffset = CGPoint(x: 0, y: -Self.canvas.height / 2)
        canShowCallout = false
        backgroundColor = .clear
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    /// 只换头色；几何与杆/高光 chrome 固定。
    /// 用 MAAnnotationView.image（勿再声明 imageView，会与基类冲突）。
    func apply(headColor: UIColor) {
        image = Self.image(headColor: headColor)
    }

    private static func image(headColor: UIColor) -> UIImage {
        let format = UIGraphicsImageRendererFormat()
        format.scale = UIScreen.main.scale
        format.opaque = false
        let renderer = UIGraphicsImageRenderer(size: canvas, format: format)
        return renderer.image { ctx in
            let cg = ctx.cgContext
            let midX = canvas.width / 2
            let tipY = head + stemLength

            // 尖端触点阴影
            cg.setFillColor(UIColor.black.withAlphaComponent(0.35).cgColor)
            cg.fillEllipse(in: CGRect(x: midX - tipDot, y: tipY - tipDot * 0.35,
                                      width: tipDot * 2, height: tipDot * 1.1))

            // 白杆（起终点同 chrome）
            cg.setStrokeColor(UIColor.white.cgColor)
            cg.setLineWidth(stemWidth)
            cg.setLineCap(.round)
            cg.move(to: CGPoint(x: midX, y: head - 0.5))
            cg.addLine(to: CGPoint(x: midX, y: tipY))
            cg.strokePath()

            // 色头
            let headRect = CGRect(x: (canvas.width - head) / 2, y: 0, width: head, height: head)
            cg.setFillColor(headColor.cgColor)
            cg.fillEllipse(in: headRect)

            // 左上白高光（specular）
            let spec: CGFloat = head * 0.28
            let specRect = CGRect(x: headRect.minX + head * 0.18,
                                  y: headRect.minY + head * 0.16,
                                  width: spec, height: spec)
            cg.setFillColor(UIColor.white.withAlphaComponent(0.92).cgColor)
            cg.fillEllipse(in: specRect)
        }
    }
}
