import Foundation
import ObjectiveC

/// Public iOS SDK bicycle planning. No driving conversion or electric-bike fallback.
/// 途经点：骑行 Search API 不支持 waypoints，按「起点→途经→…→终点」分段算路后拼接。
final class BicycleRouteQuery: NSObject {
    private static var pending: [UUID:BicycleRouteQuery]=[:]
    private let id=UUID(), request: MotorRequest
    private var api: AMapSearchAPI?
    private var continuation: CheckedContinuation<[MotorRoute],Error>?
    private var timeout: DispatchWorkItem?
    private init(_ request: MotorRequest, _ continuation: CheckedContinuation<[MotorRoute],Error>) {
        self.request=request;self.continuation=continuation;super.init()
        class_addProtocol(BicycleRouteQuery.self,AMapSearchDelegate.self)
    }
    static func load(_ request: MotorRequest) async throws -> [MotorRoute] {
        if request.vias.isEmpty {
            return try await singleLeg(request)
        }
        // 分段：每段无途经点；最多 3 途经 → 最多 4 段
        guard request.vias.count <= 3 else { throw AmapShare.Fail("途经点最多 3 个") }
        let stops = [request.start] + request.vias + [request.end]
        var legs: [MotorRoute] = []
        for i in 0..<(stops.count - 1) {
            let legReq = MotorRequest(start: stops[i], end: stops[i + 1], vias: [], bicycle: true)
            guard let leg = try await singleLeg(legReq).first else {
                throw AmapShare.Fail("非机动车途经点第 \(i + 1) 段规划失败")
            }
            legs.append(leg)
        }
        return [stitch(legs, request: request)]
    }
    private static func singleLeg(_ request: MotorRequest) async throws -> [MotorRoute] {
        try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.main.async {
                let query=BicycleRouteQuery(request,continuation)
                Self.pending[query.id]=query; query.start()
            }
        }
    }
    /// 拼接多段骑行结果为一条 MotorRoute（去掉相邻段重复的接点）。
    private static func stitch(_ legs: [MotorRoute], request: MotorRequest) -> MotorRoute {
        var points: [MotorPoint] = []
        var steps: [MotorStep] = []
        var seconds = 0
        for (li, leg) in legs.enumerated() {
            let base = points.isEmpty ? 0 : points.count - 1
            let skipFirst = li > 0
            for (pi, p) in leg.points.enumerated() {
                if skipFirst && pi == 0 { continue }
                points.append(p)
            }
            for step in leg.steps {
                let drop = skipFirst ? 1 : 0
                let start = base + max(0, step.start - drop)
                let end = base + max(0, step.end - drop)
                steps.append(MotorStep(
                    instruction: step.instruction, road: step.road, action: step.action,
                    end: end, start: start, distance: step.distance, duration: step.duration))
            }
            seconds += leg.seconds
        }
        if steps.isEmpty {
            steps = [MotorStep(instruction: "沿路线前进", road: "", action: "", end: max(0, points.count - 1))]
        }
        return MotorRoute(request: request, points: points, steps: steps, seconds: seconds)
    }
    private func start() {
        // 单段查询：本类实例的 request.vias 应为空（多段由 load 外层拆开）
        api=AMapSearchAPI(); api?.delegate=(self as AnyObject) as? AMapSearchDelegate
        let r=AMapRidingRouteSearchRequest()
        r.origin=AMapGeoPoint.location(withLatitude:CGFloat(request.start.lat),longitude:CGFloat(request.start.lng))
        r.destination=AMapGeoPoint.location(withLatitude:CGFloat(request.end.lat),longitude:CGFloat(request.end.lng))
        r.showFieldsType = .all  // v0.15.8 必须：9.7.x SDK 默认不返回分段扩展（steps 无 polyline/指令），否则"自行车路线缺少分段"
        let work=DispatchWorkItem { [weak self] in self?.finish(.failure(AmapShare.Fail("非机动车规划超时，请检查网络、定位和高德 iOS Key"))) }
        timeout=work;DispatchQueue.main.asyncAfter(deadline:.now()+25,execute:work)
        api?.aMapRidingRouteSearch(r)
    }
    private func finish(_ result: Result<[MotorRoute],Error>) {
        if !Thread.isMainThread {DispatchQueue.main.async{[weak self] in self?.finish(result)};return}
        guard let c=continuation else { return };continuation=nil
        timeout?.cancel();timeout=nil;api?.delegate=nil;api=nil
        Self.pending.removeValue(forKey:id);c.resume(with:result)
    }
    @objc(onRouteSearchDone:response:)
    func completed(_ r: AMapRouteSearchBaseRequest?, response: AMapRouteSearchResponse?) {
        do {
            let paths=response?.route?.paths ?? []
            guard !paths.isEmpty else { throw AmapShare.Fail("高德没有返回非机动车路线") }
            let routes=try paths.prefix(10).map { path -> MotorRoute in
                var points: [MotorPoint]=[], steps: [MotorStep]=[]
                for step in path.steps ?? [] {
                    let start=max(0,points.count-1)
                    for pair in (step.polyline ?? "").split(separator:";") {
                        let xy=pair.split(separator:",")
                        guard xy.count==2,let lo=Double(xy[0]),let la=Double(xy[1]),MotorRequest.valid(la,lo) else { throw AmapShare.Fail("非机动车路线坐标无效") }
                        let p=MotorPoint(lat:la,lng:lo)
                        if let previous=points.last {
                            if RouteShape.metres(previous,p)<0.01 { continue }
                            guard RouteShape.metres(previous,p)<5000 else { throw AmapShare.Fail("非机动车路线不连续") }
                        }
                        guard points.count<150000 else { throw AmapShare.Fail("非机动车路线过长") }; points.append(p)
                    }
                    guard !points.isEmpty else { continue }
                    steps.append(MotorStep(instruction:step.instruction ?? "",road:step.road ?? "",action:step.action ?? "",end:points.count-1,start:start,distance:step.distance,duration:step.duration))
                }
                guard points.count>=2,!steps.isEmpty else { throw AmapShare.Fail("非机动车路线缺少分段") }
                return MotorRoute(request:request,points:points,steps:steps,seconds:path.duration)
            }
            finish(.success(routes))
        } catch { finish(.failure(error)) }
    }
    @objc(AMapSearchRequest:didFailWithError:)
    func failed(_ request: Any?, error: NSError?) {
        finish(.failure(AmapShare.Fail("非机动车规划失败（\(error?.code ?? 0)）：\(error?.localizedDescription ?? "请检查高德 iOS Key 权限")")))
    }
}
