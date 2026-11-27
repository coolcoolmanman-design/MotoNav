import Foundation
import UIKit

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.2 新增；和安卓版 MapFeeder.kt 一样）
///
/// 小地图数据：
/// - 导航时下载当前位置附近约 2.4 公里见方的地图（高德 SDK 截图底图），
///   快开出这一块时自动下载下一块（向前方偏移，前面的路多下载一些）。
/// - 地图来源「天地图」：HTTP vec_w 瓦片 → 同套 RLE/BLE（可与高德 A/B；不恢复 SoftAP）。
/// - 0.16.2.90 混合：天地图底图 + 罪恶都市时用高德双截图叠建筑轮廓；锁屏只发已缓存包，不新开楼块提取。
/// - 选了“卫星图”风格时用天地图 img_w 卫星瓦片（半边 1000 米 → 2000m 见方，16 色 + 调色板）。
///   v0.16.2.44：高速/锁屏不再停更，按车速提前换块 + 前方剩余覆盖兜底 + 沿路瓦片预取。
/// - 导航路线（连同路上的红绿灯位置）单独打包发给圆屏。
/// - iOS v0.3 原路线（导入的高德分享路线）：大块数据类型 4，圆屏画成橙色细线；"#o" 不显示
/// - 每次定位把当前位置、朝向、车速发给圆屏："#p,坐标系,x分米,y分米,朝向,车速"。
/// - 圆屏收齐一份地图/路线后回 "K,类型,编号"；没确认的 8 秒后重发，重连后也全部重发。
/// 状态都在主线程里读写，下载和计算在后台队列里做。
final class MapFeeder: ObservableObject {
    static let shared = MapFeeder()

    private let HALF = 1200.0            // 地图块半边长（米）
    private let REFRESH = 650.0          // 离地图块中心超过这个距离就准备下一块
    private let AHEAD = 750.0            // 0.16.2.91：新块中心前移 450→750 m（加长前台预取）
    private let SAT_HALF = 1000.0        // 卫星图块半边长（米）：2000m 见方，高速视口 302/0.38≈795m 也装得下
    private let SAT_REFRESH = 300.0      // 低速换块阈值；高速由 satRefresh(kmh:) 提前到 180~250m
    private let SAT_AHEAD = 120.0        // 新块中心前移量，必须 < 最小 refresh，否则换完立刻又触发取图
    private let satWarmQueue = DispatchQueue(label: "motonav.map.satwarm", qos: .utility)
    private var lastSatWarmAt = Date.distantPast
    static let MAX_BYTES = 190_000       // 圆屏线条地图缓冲 200KB，留余量
    static let MAX_FEAT = 7000
    private let RESEND = 8.0             // 秒

    static let TYPE_CHUNK = 1
    static let TYPE_ROUTE = 2
    static let TYPE_RASTER = 3
    static let TYPE_GHOST = 4
    static let TYPE_PERSP = 13          // 0.16.2.137：原生 3D 底图（高德透视截图 + 距离表）

    static let SRC_TDT = 1
    static let SRC_AMAP = 3
    static let THEME_SAT = 10
    /// ui.6：龙珠雷达(11) 与 大字转向(12) 都不需要栅格/矢量底图。
    static let THEME_NO_BASEMAP: Set<Int> = [11, 12]
    /// musicA.59：矢量简图(13) 要真实路网，只发矢量化道路要素（bulk type 1），不发栅格。
    /// musicA.134：矢量3D鸟瞰(15) 改走 Tilt3D 栅格 + 楼块脚印，已移出本集合。
    static let THEME_ROADNET: Set<Int> = [13]

    @Published var status = "（导航时自动下载附近地图）"
    @Published var offlineStatus = "选择路线后，在前台自动准备沿线离线底图"
    @Published var chunkInfo = ""
    @Published var buildingInfo = ""
    @Published var perspInfo = ""
    /// 一帧 3D 底图当前被哪条门槛挡住（空 = 没有拦截条件）
    @Published var perspBlock = "等待定位/导航"
    @Published var routeInfo = ""
    @Published var chunkAcked = false
    @Published var routeAcked = false

    private let worker = DispatchQueue(label: "motonav.map", qos: .userInitiated)
    private(set) var frame: MapGeo.Frame?

    // 当前地图块
    private var chunkCx = 0.0, chunkCy = 0.0
    private var haveChunk = false
    private var fetching = false
    private var fetchingCenter: (x: Double, y: Double)?
    private var fetchingBaseReady = false
    private var lastFetchFail = Date.distantPast
    private var lastBuildingTry = Date.distantPast
    private var chunkId = 0
    private var nextChunkId = 0
    private var netId = 0                 // musicA.59：矢量路网要素（bulk type 1）
    private var netSerial = 0
    private var netBusy = false
    private var netKey = ""
    private var netBytes: Data?
    private var chunkType = TYPE_CHUNK
    private var chunkBytes: Data?
    private var chunkRoadBytes=0
    private var chunkStyle:Int?
    private var handoff=MapHandoffBuffer()
    private var chunkSat = false
    private var chunkSentAt = Date.distantPast

    // 当前路线
    private var routeKey = ""
    private var routeBuildSerial = 0
    private var routeId = 0
    private var routeBytes: Data?
    private var routeSentAt = Date.distantPast

    // 原路线
    private var ghostCoords: [(lat: Double, lng: Double)]?   // 还没有坐标系时先存着
    private var ghostId = 0
    private var ghostBytes: Data?
    private var ghostAcked = false
    private var ghostSentAt = Date.distantPast

    // 原生 3D 底图（P3D，bulk type 13）：高德按俯仰角渲染的透视图 + 距离→行查找表
    private var perspBytes: Data?
    private var perspId = 0
    private var perspAcked = true
    private var perspBusy = false
    private var perspSentAt = Date.distantPast
    private var perspLastTry = Date.distantPast
    private var perspAnchor: (x: Double, y: Double)?
    private var perspBearing = Double.nan

    private var lastBearing: Double?
    private var requestSerial = 0
    private var workToken = MapWorkToken()
    private var offlineMaps: RouteOfflineMaps?
    private var preparedKey = ""
    private var preparedCoords: [(lat: Double, lng: Double)] = []
    private var offlineKey = ""
    private var routeProgress = -1.0
    private var pose: (x: Double, y: Double, kmh: Int)?
    private var chunkBuildingsComplete = false
    private var lastRoofUpgradeProbe=Date.distantPast
    private var deferredRoofs:MapHandoffBuffer.Packet?
    private var upgradeBase:(packet:MapHandoffBuffer.Packet,id:Int)?
    private var confirmedCenter: (x: Double, y: Double)?
    private var lastCoverageTrace = Date.distantPast
    private var renderingAvailable=true
    /// 卫星图 / 天地图 vec_w 只走网络 + CPU，不需要高德截图 / UIKit 渲染：锁屏、息屏、SDK 渲染暂停时仍可取底图。
    private var httpBasemap: Bool { satMode || Settings.shared.mapSource == Self.SRC_TDT || lockscreenHttpFallback }
    /// 0.16.2.135：样式 15/16 用高德源时，锁屏/后台取不到截图 → 固件瓦片过期后地图整块消失。
    /// 这类主题在后台改走天地图 HTTP 底图（无楼块），回前台再恢复高德截图 + 建筑轮廓。
    private var lockscreenHttpFallback: Bool {
        guard Settings.shared.mapSource == Self.SRC_AMAP, !appActive, !satMode else { return false }
        let t = Settings.shared.theme
        return (t == 15 || t == 16) && !Settings.shared.tdtKeyInUse.isEmpty
    }
    private var appActive: Bool { renderingAvailable && UIApplication.shared.applicationState == .active }
    /// 底图：HTTP（天地图/卫星）可在锁屏取；高德截图须前台。
    private var canCapture: Bool { httpBasemap || appActive }
    /// 罪恶都市 / GTA6 / 矢量3D鸟瞰 / 昼夜通透 建筑轮廓（高德双截图）：高德或天地图底图均可叠；**仅前台**。
    private var buildingMode: Bool {
        let t = Settings.shared.theme
        return (t == 1 || t == 14 || t == 15 || t == 16)   // musicA.147c：昼夜通透也要楼块脚印
            && (Settings.shared.mapSource == Self.SRC_AMAP || Settings.shared.mapSource == Self.SRC_TDT)
            && Settings.shared.buildings
            && BleLink.shared.buildingsSupported
    }
    private var canCaptureBuildings: Bool { buildingMode && appActive }
    private var coverageSource: Bool {
        let src = Settings.shared.mapSource
        return src == Self.SRC_AMAP || src == Self.SRC_TDT
    }

    /// 车速越快越早换块，给瓦片下载 + BLE 传输留提前量。
    /// 下限 180m 必须大于 SAT_AHEAD(120m)，否则刚换完的新块会立刻被判为过期。
    private func satRefresh(kmh:Int)->Double {
        if kmh >= 100 { return 180 }
        if kmh >= 80  { return 200 }
        if kmh >= 50  { return 250 }
        return SAT_REFRESH
    }
    /// 按固件 rasterCoversView 的几何估算：视口前方剩余覆盖是否不足。
    /// 车相对图块中心在前进方向上的位移越大，前方剩得越少；不足就提前换块。
    private func satForwardShortfall(x:Double,y:Double,bearing:Double,kmh:Int)->Bool {
        guard satMode,haveChunk,chunkSat else {return false}
        let a=bearing * .pi / 180
        let fwd=(x-chunkCx)*sin(a)+(y-chunkCy)*cos(a)
        let sc=max(0.38,0.6-Double(kmh)*0.0028)
        let viewR=(233.0+29.0+40.0)/sc        // CX + |MAP_PY-CX| + 40，与固件一致
        return (SAT_HALF-fwd) < (viewR+100.0) // 额外预留 100m 给下载/传输
    }
    /// 沿导航方向预取下一块卫星瓦片到磁盘缓存（低优先级、限频，不阻塞主线程）。
    private func warmSatelliteAhead(_ f:MapGeo.Frame,x:Double,y:Double,bearing:Double) {
        guard satMode,Date().timeIntervalSince(lastSatWarmAt)>12,
              Settings.shared.tdtKeyInUse.isEmpty == false else {return}
        lastSatWarmAt=Date()
        let a=bearing * .pi / 180
        let cx=x+sin(a)*700
        let cy=y+cos(a)*700
        let half=SAT_HALF
        let key=Settings.shared.tdtKeyInUse
        satWarmQueue.async { TdtRaster.warmTiles(around:f,cx:cx,cy:cy,half:half,key:key) }
    }


    private var enabled: Bool { Settings.shared.mapOn }
    private var satMode: Bool { Settings.shared.theme == MapFeeder.THEME_SAT }

    /// Preserve prepared offline packets when navigation starts.
    func onNaviStart() { routeKey = "" }
    /// Cache reads and BLE keep running while UIKit/SDK graphics are unavailable.
    func setRenderingAvailable(_ active:Bool) {
        renderingAvailable=active
        if !active {
            // 锁屏：立刻停高德截图。若可能在叠建筑，取消在途 worker（避免锁后继续双截图）；
            // 纯 HTTP 底图（无建筑模式）可保留在途取图。
            AmapCaptureHost.shared.cancel()
            // 0.16.2.136：HTTP 底图（天地图源 / 高德锁屏回退）锁屏时仍有在途下载，
            // 不能因 buildingMode 无条件 cancel，否则瓦片被丢弃且回前台未必补发。
            let pureHttp = httpBasemap && (Settings.shared.mapSource == Self.SRC_TDT || lockscreenHttpFallback)
            if !httpBasemap || (buildingMode && !pureHttp) {
                workToken.cancel(); requestSerial += 1; fetching = false
            }
            offlineMaps?.suspendCapture()
            MapPipelineTrace.event("rendering-paused",["satMode":satMode,"tdt":Settings.shared.mapSource==Self.SRC_TDT,"buildings":buildingMode])
        } else {
            // 回前台：清失败退避，立刻补齐缺口建筑包（锁屏期间只缓存了底图）。
            lastFetchFail = .distantPast
            lastBuildingTry = .distantPast
            lastRoofUpgradeProbe = .distantPast
            offlineMaps?.resumeForegroundCatchUp()
            tryUpgradeBuildings()
            if buildingMode, let f = frame, let p = pose, !chunkBuildingsComplete, !fetching {
                fetchChunk(f, p.x, p.y)
            }
            MapPipelineTrace.event("rendering-resumed", ["buildings": buildingMode, "chunkComplete": chunkBuildingsComplete])
        }
    }
    func onNaviEnd() {
        offlineMaps?.close(); offlineMaps = nil; offlineKey = ""
        handoff.clear()
        deferredRoofs=nil;upgradeBase=nil
        preparedCoords = []; preparedKey = ""; offlineStatus = "沿线下载已暂停，重新选择路线后继续"
        workToken.cancel(); requestSerial += 1; routeBuildSerial += 1; fetching = false
    }
    func resetChunk() {
        Settings.shared.syncBuildingContinuity()
        handoff.clear();confirmedCenter=nil;deferredRoofs=nil;upgradeBase=nil;lastRoofUpgradeProbe = .distantPast
        buildingInfo = ""; lastBuildingTry = .distantPast
        resetPersp()
        workToken.cancel(); requestSerial += 1; fetching = false
        haveChunk = false; lastFetchFail = .distantPast
        chunkBytes=nil;chunkStyle=nil;chunkAcked=false
        BleLink.shared.cancelMapTransfers()
        offlineMaps?.close(); offlineMaps = nil; offlineKey = ""; routeProgress = -1
        if preparedCoords.count >= 2 { prepareRoute(key: preparedKey, coords: preparedCoords) }
        // A style change must fetch new data even while simulation/location is paused.
        let requestedStyle=Settings.shared.theme
        DispatchQueue.main.async { [weak self] in
            guard let self=self,Settings.shared.theme==requestedStyle,self.enabled,requestedStyle != 11,
                  !self.fetching,let f=self.frame,let p=self.pose else {return}
            self.fetchChunk(f,p.x,p.y)
        }
    }
    func requestMapSample() {
        guard [0,1,5].contains(Settings.shared.theme),frame != nil,pose != nil else {
            MapSampleReport.shared.fail("请先开始一次导航或模拟，再生成地图对照图。");return
        }
        let f=frame!,p=pose!
        resetChunk()
        // No cache node: the report must come from a new capture of the current area.
        fetchChunk(f,p.x,p.y)
    }
    func onBuildingCapability() {
        if Settings.shared.mapSource == Self.SRC_AMAP || Settings.shared.mapSource == Self.SRC_TDT { resetChunk() }
    }
    private func ensureFrame(lat: Double, lng: Double) -> MapGeo.Frame {
        if let f = frame { return f }
        let la = (lat * 10).rounded() / 10, lo = (lng * 10).rounded() / 10
        let id = ((Int((la * 10).rounded()) * 7919 + Int((lo * 10).rounded()) * 104729) & 0x7FFF) | 1
        let f = MapGeo.Frame(refLat: la, refLng: lo, id: id); frame = f; return f
    }
    /// Called when selecting a route, before departure, and after a reroute.
    func prepareRoute(key: String, coords: [(lat: Double, lng: Double)]) {
        guard coords.count >= 2 else { return }
        preparedKey = key; preparedCoords = coords
        let f = ensureFrame(lat: coords[0].lat, lng: coords[0].lng), src = Settings.shared.mapSource
        guard enabled, !satMode, !Self.THEME_NO_BASEMAP.contains(Settings.shared.theme) else { return }
        var hasher = Hasher()
        for c in coords { hasher.combine(c.lat); hasher.combine(c.lng) }
        let identity = "\(key)|\(hasher.finalize())|\(src)|\(buildingMode)"
        if identity == offlineKey, offlineMaps != nil { return }
        handoff.clear()
        offlineMaps?.close(); offlineKey = identity; routeProgress = -1
        workToken.cancel(); requestSerial += 1; fetching = false
        do {
            let plan = try RouteMapPlan(coords.map { (x: f.x($0.lng), y: f.y($0.lat)) })
            let maps = RouteOfflineMaps(frame: f, plan: plan, source: src, buildings: buildingMode,
                foregroundBusy: { [weak self] in self?.fetching ?? true },
                currentSpeed: { [weak self] in self?.pose?.kmh ?? 0 },
                onReady: { [weak self] node in
                    guard let self = self, self.offlineKey == identity, self.enabled else { return }
                    if self.haveChunk,abs(self.chunkCx-node.x)<1,abs(self.chunkCy-node.y)<1 {
                        self.tryUpgradeBuildings();return
                    }
                    let progress = self.pose.map { plan.progress(x: $0.x, y: $0.y, previous: self.routeProgress) } ?? 0
                    let onRoute = plan.at(progress)
                    if let p = self.pose, hypot(p.x - onRoute.x, p.y - onRoute.y) >= 400 { return }
                    let wantedIndex = self.pose.map { plan.displayTarget(progress: progress, x: $0.x, y: $0.y, kmh: $0.kmh) }
                        ?? plan.target(progress: progress, initial: true)
                    let wanted = plan.nodes[wantedIndex]
                    guard wanted == node else { return }
                    if self.haveChunk && abs(self.chunkCx - node.x) < 1 && abs(self.chunkCy - node.y) < 1 {
                        if !self.buildingMode || self.chunkBuildingsComplete { return }
                        if let packet = self.offlineMaps?.read(node), !RasterPacket.isBuilding(packet) { return }
                    }
                    if !self.useCachedMap(f,node:node),self.canCapture,!self.fetching {
                        self.fetchChunk(f,node.x,node.y,node:node)
                    }
                }, onStatus: { [weak self] status in
                    if self?.offlineKey == identity { self?.offlineStatus = status }
                })
            offlineMaps = maps; maps.start()
        } catch { offlineStatus = "沿线离线地图：\(error.localizedDescription)" }
    }
    func onLocation(lat: Double, lng: Double, bearing: Double, kmh: Int) {
        let f = ensureFrame(lat: lat, lng: lng), x = f.x(lng), y = f.y(lat)
        pose = (x, y, kmh)
        if !canCapture,fetching {
            workToken.cancel();requestSerial += 1;fetching=false;offlineMaps?.suspendCapture()
        }
        protectBaseDuringRoofTransfer()
        flushPreparedMap()
        if enabled, Date().timeIntervalSince(lastCoverageTrace) >= 2 {
            lastCoverageTrace = Date()
            MapPipelineTrace.event("view-coverage", ["kmh": kmh, "ready": BleLink.shared.isReady,
                "sat": satMode, "satHalfM": SAT_HALF, "satRefreshM": satRefresh(kmh: kmh),
                "sentCovered": haveChunk && MapCoveragePolicy.covers(x: x, y: y, cx: chunkCx, cy: chunkCy, kmh: max(120, kmh)),
                "bytes":chunkBytes?.count ?? 0,"awaitAck":!chunkAcked,
                "transferPending":BleLink.shared.bulkPending(type:chunkType,id:chunkId),
                "preparedOffsetM":handoff.pending.map{max(abs(x-$0.cx),abs(y-$0.cy))} ?? -1,
                "viewportOffsetM":confirmedCenter.map{max(abs(x-$0.x),abs(y-$0.y))} ?? -1,
                "confirmedCovered": confirmedCenter.map { MapCoveragePolicy.covers(x: x, y: y, cx: $0.x, cy: $0.y, kmh: max(120, kmh)) } ?? false])
        }
        var hdg = bearing
        if kmh >= 4 || lastBearing == nil { if kmh >= 4 { lastBearing = bearing } } else { hdg = lastBearing! }
        BleLink.shared.send("#p,\(f.id),\(Int((x * 10).rounded())),\(Int((y * 10).rounded())),\(Int(hdg.rounded())),\(kmh)")
        if ghostCoords != nil && ghostBytes == nil { encodeGhost() }
        if !enabled { return }
        maybePushPersp(x: x, y: y, bearing: hdg, kmh: kmh)
        if Self.THEME_NO_BASEMAP.contains(Settings.shared.theme) {
            status = Settings.themeName(Settings.shared.theme) + "（无需底图）"; checkResend(); return
        }
        var node: RouteMapPlan.Node?
        var futureNode:RouteMapPlan.Node?
        if let maps = offlineMaps {
            routeProgress = maps.plan.progress(x: x, y: y, previous: routeProgress)
            let projected = maps.plan.at(routeProgress), index = maps.plan.displayTarget(progress: routeProgress, x: x, y: y, kmh: kmh)
            let preparationIndex=maps.plan.preparationTarget(progress:routeProgress,kmh:kmh)
            maps.update(preparationIndex,display:index)
            let candidate = maps.plan.nodes[index]
            if hypot(x - projected.x, y - projected.y) < 400,
               MapCoveragePolicy.covers(x: x, y: y, cx: candidate.x, cy: candidate.y, kmh: max(120, kmh), margin: 50) {
                node = candidate
                futureNode=maps.plan.nodes[preparationIndex]
            }
        }
        if let current=node {
            var relevant=[(x:current.x,y:current.y)]
            if let future=futureNode {relevant.append((x:future.x,y:future.y))}
            if handoff.discardObsolete(x:x,y:y,kmh:kmh,allowed:relevant) {
                MapPipelineTrace.event("staged-map-expired",["kmh":kmh])
            }
        }
        let confirmedCovered=confirmedCenter.map{MapCoveragePolicy.covers(x:x,y:y,cx:$0.x,cy:$0.y,kmh:max(120,kmh),margin:50)} ?? false
        let recovering=haveChunk && !confirmedCovered
        let distance = hypot(x - chunkCx, y - chunkCy)
        let refresh = chunkSat ? satRefresh(kmh: kmh) : REFRESH
        let satShortfall = satForwardShortfall(x:x,y:y,bearing:bearing,kmh:kmh)
        let changedNode = node.map { abs($0.x - chunkCx) >= 1 || abs($0.y - chunkCy) >= 1 } ?? false
        let needsCoverage = !satMode && coverageSource && !MapCoveragePolicy.covers(x: x, y: y, cx: chunkCx, cy: chunkCy, kmh: max(120, kmh), margin: 50)
        let prepareEarly = !satMode && coverageSource &&
            MapCoveragePolicy.needsRefresh(x: x, y: y, cx: chunkCx, cy: chunkCy, bearing: bearing, kmh: kmh)
        // A building pass must yield before movement leaves its base map. Cached bases can switch immediately.
        // 天地图无建筑二阶段；仅在高德且已出基础底图时按移动取消。
        // 0.16.2.136：样式16天地图源也会走高德建筑双截图，移动时同样要能打断，
        // 否则在途建筑提取阻塞新块 fetch（376 行 !fetching 门槛）→ 旧瓦片过期后补不上。
        if fetching, !satMode, let center = fetchingCenter,
           (fetchingBaseReady && (needsCoverage || changedNode) || !MapHandoffBuffer.mayPrepare(x:x,y:y,cx:center.x,cy:center.y)) {
            MapPipelineTrace.event("movement-cancel", ["serial": requestSerial, "baseReady": fetchingBaseReady, "needsCoverage": needsCoverage, "nextBlock": changedNode, "kmh": kmh])
            workToken.cancel(); requestSerial += 1; fetching = false
        }
        let retryBuildings = offlineMaps == nil && canCaptureBuildings && MapCoveragePolicy.mayCaptureBuildings(kmh:kmh) && !chunkBuildingsComplete && Date().timeIntervalSince(lastBuildingTry) >= 60
        // Preparation and display targets differ: a future block may be captured now,
        // then held until it covers the viewport and the previous ATT transfer finishes.
        let wanted = haveChunk && prepareEarly && !recovering ? (futureNode ?? node) : node
        let a = bearing * .pi / 180, ahead = kmh > 5 ? (satMode ? SAT_AHEAD : MapCoveragePolicy.lookahead(kmh: kmh, preferred: AHEAD)) : 0
        let targetX=wanted?.x ?? (x+sin(a)*ahead), targetY=wanted?.y ?? (y+cos(a)*ahead)
        let sameCurrent=haveChunk && abs(targetX-chunkCx)<1 && abs(targetY-chunkCy)<1
        let alreadyPrepared=handoff.contains(cx:targetX,cy:targetY,style:Settings.shared.theme)
        let needsMap = !haveChunk || distance > refresh || needsCoverage || changedNode || (prepareEarly && !sameCurrent) || retryBuildings || satShortfall
        var servedCache=false
        if needsMap,!alreadyPrepared {
            let cacheNode=wanted ?? RouteMapPlan.Node(x:(targetX/2).rounded(.toNearestOrEven)*2,y:(targetY/2).rounded(.toNearestOrEven)*2,distance:0)
            servedCache=useCachedMap(f,node:cacheNode)
            if !servedCache,!canCapture,!httpBasemap,!confirmedCovered,
               let nearby=RouteMapCache.shared.covering(frame:f,source:Settings.shared.mapSource,style:Settings.shared.theme,x:x,y:y,kmh:kmh) {
                acceptChunk(nearby.bytes,cx:nearby.node.x,cy:nearby.node.y,sat:false,type:Self.TYPE_RASTER,info:"复用附近已缓存底图",src:Settings.shared.mapSource)
                servedCache=true
            }
        }
        let backoff = httpBasemap ? 6.0 : 15.0   // HTTP 瓦片失败重试更快
        if canCapture,!servedCache,!fetching && handoff.pending == nil && !alreadyPrepared && needsMap && Date().timeIntervalSince(lastFetchFail) > backoff {
            fetchChunk(f,targetX,targetY,node:wanted)
        }
        if !canCapture,needsMap,!servedCache,!confirmedCovered {
            status="此处底图尚未缓存，保留已有地图；回前台补充"
        }
        warmSatelliteAhead(f,x:x,y:y,bearing:bearing)
        tryUpgradeBuildings();checkResend()
    }
    private func roofDeliveryAllowed(_ bytes:Data,cx:Double,cy:Double)->Bool {
        guard haveChunk,chunkAcked,let p=pose,let center=confirmedCenter,
              abs(center.x-chunkCx)<1,abs(center.y-chunkCy)<1 else {return false}
        let seconds=MapCoveragePolicy.remainingSeconds(x:p.x,y:p.y,cx:center.x,cy:center.y,
            kmh:p.kmh,bearing:lastBearing,plan:offlineMaps?.plan,progress:routeProgress)
        let candidateSeconds=MapCoveragePolicy.remainingSeconds(x:p.x,y:p.y,cx:cx,cy:cy,
            kmh:p.kmh,bearing:lastBearing,plan:offlineMaps?.plan,progress:routeProgress)
        guard seconds>0,candidateSeconds>0 else {return false}
        // Admission and in-flight protection reserve the same essential-road
        // budget, including a new centre. Roof bytes are never a road reserve.
        let reserve=max(chunkRoadBytes,handoff.pending?.roadBytes ?? 0)
        return MapDeliveryPolicy.roofsFit(kmh:p.kmh,bytes:bytes.count,rate:BleLink.shared.mapBytesPerSecond,
            coverageSeconds:min(seconds,candidateSeconds),nextBaseBytes:reserve)
    }
    /// A detail transfer begun while stopped must yield if departure makes its
    /// remaining bytes unsafe. Restore the genuinely acknowledged base state;
    /// do not claim the abandoned building packet was received.
    private func protectBaseDuringRoofTransfer() {
        guard !chunkAcked,let saved=upgradeBase,let p=pose else {return}
        let remaining=BleLink.shared.mapPayloadRemaining(id:chunkId)
        guard remaining>0 else {return} // Already awaiting K: let bounded receipt recovery finish.
        let old=saved.packet
        let seconds=MapCoveragePolicy.remainingSeconds(x:p.x,y:p.y,cx:old.cx,cy:old.cy,
            kmh:p.kmh,bearing:lastBearing,plan:offlineMaps?.plan,progress:routeProgress)
        let candidateSeconds=MapCoveragePolicy.remainingSeconds(x:p.x,y:p.y,cx:chunkCx,cy:chunkCy,
            kmh:p.kmh,bearing:lastBearing,plan:offlineMaps?.plan,progress:routeProgress)
        let reserve=max(old.roadBytes,handoff.pending?.roadBytes ?? 0)
        guard min(seconds,candidateSeconds)<=0 || !MapDeliveryPolicy.roofsFit(kmh:p.kmh,bytes:remaining,
            rate:BleLink.shared.mapBytesPerSecond,coverageSeconds:min(seconds,candidateSeconds),nextBaseBytes:reserve) else {return}
        MapPipelineTrace.event("roof-yield",["id":chunkId,"bytes":remaining,"kmh":p.kmh])
        if let bytes=chunkBytes,RasterPacket.isBuilding(bytes) {
            deferredRoofs = .init(bytes:bytes,cx:chunkCx,cy:chunkCy,sat:chunkSat,type:chunkType,info:"暂缓建筑包",source:Settings.shared.mapSource,style:chunkStyle ?? Settings.shared.theme)
        }
        BleLink.shared.cancelMapTransfers()
        chunkBytes=old.bytes;chunkRoadBytes=old.roadBytes;chunkCx=old.cx;chunkCy=old.cy;chunkId=saved.id
        chunkType=old.type;chunkSat=old.sat;chunkStyle=old.style
        chunkAcked=true;chunkBuildingsComplete=RasterPacket.isBuilding(old.bytes)
        confirmedCenter=(old.cx,old.cy);upgradeBase=nil;lastRoofUpgradeProbe=Date()
        status="保留已确认底图，优先准备前方道路";chunkInfo="地图块 #\(chunkId)：已确认底图"
    }
    private func tryUpgradeBuildings() {
        guard enabled,buildingMode,haveChunk,chunkAcked,!chunkBuildingsComplete,!fetching,
              BleLink.shared.isReady,!BleLink.shared.bulkPending,
              Date().timeIntervalSince(lastRoofUpgradeProbe)>=2 else {return}
        lastRoofUpgradeProbe=Date()
        let saved=deferredRoofs.flatMap{p -> MapHandoffBuffer.Packet? in
            p.style==Settings.shared.theme && abs(p.cx-chunkCx)<1 && abs(p.cy-chunkCy)<1 ? p:nil
        }
        let candidate:MapHandoffBuffer.Packet?
        if let saved=saved {candidate=saved}
        else if let maps=offlineMaps,let node=maps.plan.nodes.first(where:{abs($0.x-chunkCx)<1 && abs($0.y-chunkCy)<1}),
                let bytes=maps.read(node),RasterPacket.isBuilding(bytes) {
            candidate = .init(bytes:bytes,cx:node.x,cy:node.y,sat:false,type:Self.TYPE_RASTER,info:"沿线建筑包",source:Settings.shared.mapSource,style:Settings.shared.theme)
        } else {candidate=nil}
        guard let candidate=candidate else {return}
        guard roofDeliveryAllowed(candidate.bytes,cx:candidate.cx,cy:candidate.cy) else {
            MapPipelineTrace.event("roof-deferred",["bytes":candidate.bytes.count,"kmh":pose?.kmh ?? 0,"rate":BleLink.shared.mapBytesPerSecond ?? 0]);return
        }
        MapPipelineTrace.event("roof-upgrade",["bytes":candidate.bytes.count,"kmh":pose?.kmh ?? 0,"rate":BleLink.shared.mapBytesPerSecond ?? 0])
        acceptChunk(candidate.bytes,cx:candidate.cx,cy:candidate.cy,sat:candidate.sat,type:candidate.type,info:candidate.info,src:candidate.source)
    }
    private func acceptChunk(_ incoming: Data, cx: Double, cy: Double, sat: Bool, type: Int, info: String, src: Int) {
        guard enabled, !Self.THEME_NO_BASEMAP.contains(Settings.shared.theme) else { return }
        let kmh=pose?.kmh ?? 0
        if buildingMode,RasterPacket.isBuilding(incoming) {
            deferredRoofs = .init(bytes:incoming,cx:cx,cy:cy,sat:sat,type:type,info:info,source:src,style:Settings.shared.theme)
        }
        let allowRoofs=buildingMode && roofDeliveryAllowed(incoming,cx:cx,cy:cy)
        let styled = sat ? incoming : MapStyleTransfer.adapt(incoming, style: Settings.shared.theme)
        let b=allowRoofs ? styled:MapDeliveryPolicy.compactBase(styled)
        if b.count<incoming.count {
            MapPipelineTrace.event("map-base-priority",["beforeBytes":incoming.count,"bytes":b.count,"kmh":kmh])
            if kmh>=60 {buildingInfo="高速行驶优先保证底图，稍后补充建筑"}
        }
        guard sat || MapStyleTransfer.matches(b,style:Settings.shared.theme) else {return}
        guard !(src == Self.SRC_AMAP || src == Self.SRC_TDT) || Settings.shared.theme != 1 || RasterPacket.hasMapContent(b) else {
            status="底图尚未准备完整，保留已有地图";return
        }
        let complete = RasterPacket.isBuilding(b)
        // 0.16.2.136：若旧包已 ACK 且发送超过 RESEND 仍无新确认，视为固件侧丢失，允许底图替换。
        let ackLost = haveChunk && chunkAcked && Date().timeIntervalSince(chunkSentAt) > 8.0
        if haveChunk,chunkType==type,chunkSat==sat,chunkBuildingsComplete,!complete,
           abs(cx-chunkCx)<1,abs(cy-chunkCy)<1, !ackLost {
            // A repeated base callback is not a newer building observation.
            // Preserve already-delivered or in-flight detail at the same location.
            MapPipelineTrace.event("roof-downgrade-blocked",["id":chunkId]);return
        }
        if haveChunk, chunkType == type, chunkSat == sat, abs(cx - chunkCx) < 1, abs(cy - chunkCy) < 1,
           !complete, !chunkAcked, Date().timeIntervalSince(chunkSentAt) <= 8.0 {
            MapPipelineTrace.event("base-kept-inflight", ["id": chunkId])
            return
        }
        // The complete incoming detail packet was budgeted; do not inflate it
        // by carrying extra roofs from an older map after that decision.
        let displayBytes = b
        if haveChunk, chunkType == type, chunkSat == sat, abs(cx - chunkCx) < 1, abs(cy - chunkCy) < 1,
           displayBytes == chunkBytes, chunkAcked {
            // Cached base then screenshot base often encode the exact same map.
            // Keep the transfer ID so the second callback cannot invalidate fragments already received.
            chunkBuildingsComplete = chunkBuildingsComplete || complete
            return
        }
        let transferPending = haveChunk && !chunkAcked && BleLink.shared.bulkPending(type:chunkType,id:chunkId)
        let outsideView = pose.map { !sat && (src == Self.SRC_AMAP || src == Self.SRC_TDT) && !MapCoveragePolicy.covers(x:$0.x,y:$0.y,cx:cx,cy:cy,kmh:max(120,$0.kmh),margin:50) } ?? false
        if transferPending || outsideView {
            if pose.map({MapHandoffBuffer.mayPrepare(x:$0.x,y:$0.y,cx:cx,cy:cy)}) ?? true {
                // Keep cached roofs until the packet actually becomes displayable.
                // A premature compact-only stage forces a second raster decode even
                // when the full packet later fits the measured delivery window.
                handoff.stage(.init(bytes:incoming,cx:cx,cy:cy,sat:sat,type:type,info:info,source:src,style:Settings.shared.theme))
                MapPipelineTrace.event("map-staged",["transferPending":transferPending,"outsideView":outsideView])
            }
            return
        }
        if handoff.contains(cx:cx,cy:cy,style:Settings.shared.theme) {handoff.clear()}
        if complete,haveChunk,chunkAcked,let previous=chunkBytes {
            upgradeBase=(.init(bytes:previous,cx:chunkCx,cy:chunkCy,sat:chunkSat,type:chunkType,info:"已确认底图",source:src,style:chunkStyle ?? Settings.shared.theme),chunkId)
        } else {upgradeBase=nil}
        chunkBytes = displayBytes
        chunkRoadBytes=MapDeliveryPolicy.compactBase(displayBytes).count
        chunkStyle=Settings.shared.theme
        chunkBuildingsComplete = complete
        if complete,deferredRoofs?.cx==cx,deferredRoofs?.cy==cy {deferredRoofs=nil}
        haveChunk = true; chunkSat = sat; chunkCx = cx; chunkCy = cy
        nextChunkId = (nextChunkId + 1) & 0xFF;chunkId=nextChunkId;chunkType = type; chunkAcked = false
        status = complete ? "高德建筑底图已就绪" : "地图已就绪"
        chunkInfo = "地图块 #\(chunkId)：\(info)，\(b.count / 1024) KB" + (buildingMode && !complete ? "（建筑尚未完成，显示普通底图）" : "")
        sendChunk()
    }
    private func useCachedMap(_ f:MapGeo.Frame,node:RouteMapPlan.Node)->Bool {
        guard !satMode,!Self.THEME_NO_BASEMAP.contains(Settings.shared.theme) else {return false}
        // 0.16.2.136：未确认的旧包不能挡住缓存刷新（固件丢 K 后靠这里补发）
        if haveChunk,chunkAcked,chunkStyle==Settings.shared.theme,chunkBuildingsComplete,abs(chunkCx-node.x)<1,abs(chunkCy-node.y)<1 {return true}
        let cache=RouteMapCache.shared,src=Settings.shared.mapSource,style=Settings.shared.theme
        let roofs=buildingMode ? cache.read(frame:f,source:src,buildings:true,node:node,style:style):nil
        guard let bytes=roofs ?? cache.read(frame:f,source:src,buildings:false,node:node,style:style) else {return false}
        acceptChunk(bytes,cx:node.x,cy:node.y,sat:false,type:Self.TYPE_RASTER,info:"沿线缓存底图",src:src)
        MapPipelineTrace.event("cache-map-selected",["buildings":RasterPacket.isBuilding(bytes),"bytes":bytes.count])
        return true
    }
    private func fetchChunk(_ f: MapGeo.Frame, _ requestedX: Double, _ requestedY: Double, node: RouteMapPlan.Node? = nil) {
        let cacheNode=node ?? RouteMapPlan.Node(x:(requestedX/2).rounded(.toNearestOrEven)*2,y:(requestedY/2).rounded(.toNearestOrEven)*2,distance:0)
        let cx=cacheNode.x,cy=cacheNode.y
        // Cached maps are independent of screenshot jobs and their failure backoff.
        if !canCapture { _=useCachedMap(f,node:cacheNode);return }
        fetching = true; status = "正在准备附近地图…"; requestSerial += 1
        fetchingCenter = (cx, cy); fetchingBaseReady = false
        let serial = requestSerial
        MapPipelineTrace.event("fetch-start", ["serial": serial, "buildings": buildingMode, "source": Settings.shared.mapSource])
        workToken.cancel(); let token = MapWorkToken(); workToken = token
        let src = Settings.shared.mapSource, sat = satMode, style = Settings.shared.theme
        // 建筑双截图仅前台；锁屏只走缓存/HTTP 底图，绝不新开高德楼块提取。
        let buildings = node == nil && canCaptureBuildings && MapCoveragePolicy.mayCaptureBuildings(kmh: pose?.kmh ?? 0)
        let half = HALF, satHalf = SAT_HALF
        let tdtKey = Settings.shared.tdtKeyInUse        // 主线程取值，后台只做网络/编码
        // 0.16.2.135：锁屏回退天地图时，底图包按 SRC_TDT 存/取，避免污染高德缓存
        let tdtFallback = lockscreenHttpFallback
        let baseSrc = tdtFallback ? Self.SRC_TDT : src
        // Cached packets can be read and sent even when iOS has suspended graphics rendering.
        let cached=buildingMode ? RouteMapCache.shared.read(frame:f,source:src,buildings:true,node:cacheNode,style:style)
            ?? RouteMapCache.shared.read(frame:f,source:src,buildings:false,node:cacheNode,style:style)
            : RouteMapCache.shared.read(frame:f,source:src,buildings:false,node:cacheNode,style:style)
        if let cached = cached,!sat {
            acceptChunk(cached, cx: cx, cy: cy, sat: false, type: Self.TYPE_RASTER, info: "沿线离线包", src: src)
            fetchingBaseReady = true
            if !buildings || RasterPacket.isBuilding(cached) || !canCaptureBuildings { fetching = false; return }
            // A cached base is usable immediately, but must not suppress building extraction.
        }
        if buildings { lastBuildingTry = Date() }
        if !canCapture {
            fetching = false
            if buildingMode && !appActive {
                buildingInfo = "回到 App 前台继续提取建筑；已保存底图仍可使用"
            }
            return
        }
        // 锁屏：可继续天地图 HTTP 底图，但禁止建筑提取（上面 buildings 已为 false）
        if buildingMode && !buildings && !appActive {
            buildingInfo = tdtFallback
                ? "锁屏改用天地图网络底图（无楼块）；回前台恢复高德底图与建筑轮廓"
                : "锁屏只用已缓存地图包；回前台再提取高德建筑轮廓"
        } else if !appActive && !satMode && cached == nil {
            // 锁屏且没有天地图 Key：高德截图取不到，只能靠沿线缓存
            if Settings.shared.mapSource == Self.SRC_AMAP && Settings.shared.tdtKeyInUse.isEmpty {
                buildingInfo = "锁屏无法取高德底图：配置天地图 Key 或把地图来源切到天地图（musicA.150 样式 15/16 推荐）"
            }
        }
        worker.async {
            var bytes: Data?, type = Self.TYPE_RASTER, gotSat = false, errorText = "", info = ""
            do {
                if token.isCancelled { throw MapProcessingError.cancelled }
                if sat {
                    bytes = try TdtRaster.buildSat(f, cx: cx, cy: cy, half: satHalf, key: tdtKey).bytes
                    gotSat = true; info = "天地图卫星图（img_w）"
                } else if src == Self.SRC_TDT || tdtFallback {
                    // 底图：天地图 vec_w；可选叠高德建筑（仅 buildings==true / 前台）
                    // TDT always emits classified flags=2000 — adapt for style 0/5 before match/store.
                    let baseCached = RouteMapCache.shared.read(frame: f, source: baseSrc, buildings: false, node: cacheNode, style: style)
                        ?? (cached.flatMap { RasterPacket.isBuilding($0) ? nil : $0 })
                    let basePacket: Data
                    if let baseCached, RasterPacket.valid(baseCached) {
                        basePacket = MapStyleTransfer.adapt(baseCached, style: style)
                        info = "天地图缓存底图"
                    } else {
                        let r = try TdtRaster.build(f, cx: cx, cy: cy, half: half, key: tdtKey)
                        basePacket = MapStyleTransfer.adapt(r.bytes, style: style)
                        info = "天地图 vec_w（\(r.tiles) 瓦片，缓存 \(r.fromCache)）"
                    }
                    try? RouteMapCache.shared.store(basePacket, frame: f, source: baseSrc, node: cacheNode, style: style)
                    DispatchQueue.main.async {
                        if serial == self.requestSerial && !token.isCancelled {
                            self.fetchingBaseReady = true
                            self.acceptChunk(basePacket, cx: cx, cy: cy, sat: false, type: Self.TYPE_RASTER, info: info, src: src)
                        }
                    }
                    if buildings {
                        let hybrid = try AmapSdkRaster.overlayBuildings(onto: basePacket, frame: f, shouldCancel: { token.isCancelled }, onBuildingStatus: { message in
                            DispatchQueue.main.async {
                                if serial == self.requestSerial && !token.isCancelled { self.buildingInfo = message }
                            }
                        })
                        bytes = hybrid.bytes
                        if RasterPacket.isBuilding(hybrid.bytes) { info = "天地图底图 + 高德建筑轮廓" }
                    } else {
                        bytes = basePacket
                    }
                } else if src == Self.SRC_AMAP {
                    bytes = try AmapSdkRaster.build(f, cx: cx, cy: cy, includeBuildings: buildings, shouldCancel: { token.isCancelled }, onBaseReady: { base in
                        try? RouteMapCache.shared.store(base, frame: f, source: src, node: cacheNode, style: style)
                        DispatchQueue.main.async {
                            if serial == self.requestSerial && !token.isCancelled {
                                self.fetchingBaseReady = true
                                self.acceptChunk(base, cx: cx, cy: cy, sat: false, type: Self.TYPE_RASTER, info: "高德无字底图，2 米/像素", src: src)
                            }
                        }
                    }, onBuildingStatus: { message in
                        DispatchQueue.main.async {
                            if serial == self.requestSerial && !token.isCancelled { self.buildingInfo = message }
                        }
                    }, style: style,preparedBase:cached).bytes
                    info = "高德无字底图，2 米/像素"
                } else {
                    throw TdtRaster.Fail("未知地图来源 \(src)")
                }
                if let packet = bytes, !sat, !token.isCancelled {
                    try? RouteMapCache.shared.store(packet, frame: f, source: src, node: cacheNode, style: style)
                }
            } catch { errorText = error.localizedDescription }
            DispatchQueue.main.async {
                guard serial == self.requestSerial, !token.isCancelled else { return }
                self.fetching = false
                guard let packet = bytes else {
                    self.lastFetchFail = Date();self.status = "地图准备失败，保留已有地图：\(errorText)"
                    MapSampleReport.shared.failIfWaiting("本次取图未完成，请保持 App 前台后重试。");return
                }
                self.acceptChunk(packet, cx: cx, cy: cy, sat: gotSat, type: type, info: info, src: src)
            }
        }
    }

    private func flushPreparedMap() {
        guard enabled,let p=pose else {return}
        let blocked=haveChunk && !chunkAcked && BleLink.shared.bulkPending(type:chunkType,id:chunkId)
        if let next=handoff.take(x:p.x,y:p.y,kmh:p.kmh,style:Settings.shared.theme,transferPending:blocked) {
            acceptChunk(next.bytes,cx:next.cx,cy:next.cy,sat:next.sat,type:next.type,info:next.info,src:next.source)
        }
    }
    private func sendChunk() {
        guard let b = chunkBytes, enabled, !Self.THEME_NO_BASEMAP.contains(Settings.shared.theme),chunkStyle==Settings.shared.theme else { return }
        // musicA.59：矢量简图不上屏栅格，改把道路层矢量化后走 bulk type 1。
        if Self.THEME_ROADNET.contains(Settings.shared.theme) { sendRoadNet(b); return }
        chunkSentAt = Date()
        let wire = BleLink.shared.buildingsSupported ? b : RasterPacket.forLegacy(b)
        MapPipelineTrace.packet(wire, type: chunkType, id: chunkId, ready: BleLink.shared.isReady)
        BleLink.shared.sendBulk(type: chunkType, id: chunkId, data: wire)
    }

    /// 矢量简图（13）：把刚抓到的栅格底图的道路标签层抽成折线，只发要素。
    /// 栅格包留在本地做矢量化输入，不上 BLE；要素包 10 KB 级，导航中不会挤掉路线与路口图。
    private func sendRoadNet(_ packet: Data) {
        guard let f = frame else { return }
        // 建筑包的索引是建筑类型而不是道路标签，等基础底图那一版；已发出的路网保持不动。
        guard !RasterPacket.isBuilding(packet) else {
            if netBytes != nil { return }
            status = "等基础底图，暂缓路网"
            return
        }
        let key = "\(chunkId)-\(Int(chunkCx))-\(Int(chunkCy))"
        if let cached = netBytes, netKey == key {           // 同一块只算一次，重发直接复用
            chunkSentAt = Date()
            netId = (netId + 1) & 0xFF
            BleLink.shared.sendBulk(type: Self.TYPE_CHUNK, id: netId, data: cached)
            return
        }
        guard !netBusy else { return }
        netBusy = true
        chunkSentAt = Date()                                 // 矢量化期间不要被 checkResend 打断
        netSerial += 1
        let serial = netSerial
        let cx = chunkCx, cy = chunkCy
        let style = Settings.shared.theme
        worker.async { [weak self] in
            guard let self else { return }
            var out: RoadNetVectorizer.Output?
            if self.netSerial == serial, self.enabled,
               let idx = RasterPacket.decode(packet) {
                let head = [UInt8](packet)
                let x0 = Double(RasterPacket.i32(head, 2)), y0 = Double(RasterPacket.i32(head, 6))
                let w = RasterPacket.u16(head, 10), h = RasterPacket.u16(head, 12)
                out = RoadNetVectorizer.build(frameId: f.id, idx: idx, w: w, h: h,
                                              originX: x0, originY: y0, mpp: 2.0,
                                              maxBytes: 48_000, maxFeat: 1_400)
            }
            DispatchQueue.main.async {
                self.netBusy = false
                self.chunkSentAt = Date()
                guard self.netSerial == serial, self.enabled,
                      Settings.shared.theme == style,
                      Self.THEME_ROADNET.contains(Settings.shared.theme),
                      abs(self.chunkCx - cx) < 1, abs(self.chunkCy - cy) < 1 else { return }
                guard let out else {
                    self.status = "路网要素提取失败，保留主路线"
                    return
                }
                self.netBytes = out.data
                self.netKey = key
                self.netId = (self.netId + 1) & 0xFF
                BleLink.shared.sendBulk(type: Self.TYPE_CHUNK, id: self.netId, data: out.data)
                let kb = String(format: "%.1f", Double(out.data.count) / 1024.0)
                self.status = "路网已就绪：\(out.lines) 条 · \(out.points) 点 · \(kb) KB"
            }
        }
    }


    // MARK: - 原生 3D 底图（P3D，bulk type 13）

    /// 「现在能不能发一帧 3D 底图」：nil = 可以，否则返回被拦的原因（设置页原样显示）。
    /// force = 设置页「立即传一帧」，只跳过 7s / 40m / 块中心三个节流，其它门槛照旧。
    func perspGate(x: Double, y: Double, bearing: Double, force: Bool) -> String? {
        let theme = Settings.shared.theme
        if !Settings.shared.persp3D { return "开关未开" }
        if !enabled { return "小地图已关闭" }
        if Settings.shared.mapSource != Self.SRC_AMAP { return "底图源是天地图（P3D 只走高德）" }
        if theme != 15 && theme != 16 { return "主题 \(theme) 不是 15/16" }
        if !appActive { return "App 不在前台（锁屏不抓图）" }
        if frame == nil { return "还没有坐标系（等定位）" }
        if !haveChunk { return "还没抓到首张底图" }
        if !chunkAcked { return "圆屏还没确认底图" }
        if perspBusy { return "上一帧还在抓图" }
        if fetching { return "正在抓新底图" }
        if handoff.pending != nil { return "有底图在排队待发" }
        if !BleLink.shared.isReady { return "蓝牙未连接" }
        if BleLink.shared.bulkPending { return "蓝牙队列忙（底图/路线在传）" }
        if force { return nil }
        let distance = hypot(x - chunkCx, y - chunkCy)
        if distance >= REFRESH * 0.5 { return String(format: "离块中心 %.0fm（≥%.0fm 就换块）", distance, REFRESH * 0.5) }
        let since = Date().timeIntervalSince(perspLastTry)
        if since < 7 { return String(format: "距上一帧 %.0fs，要 ≥7s", since) }
        var delta = abs(bearing - (perspBearing.isNaN ? bearing : perspBearing)).truncatingRemainder(dividingBy: 360)
        if delta > 180 { delta = 360 - delta }
        let moved = perspAnchor.map { hypot(x - $0.x, y - $0.y) } ?? 1e9
        // 停着不动就别重复传：朝向变 10° 或挪了 40m 才换一帧
        if perspBytes != nil, moved < 40, delta < 10 {
            return String(format: "没挪 40m（%.0fm）也没转 10°（%.0f°）", moved, delta)
        }
        return nil
    }

    /// 只在样式 15/16 + 高德源 + 前台 + 底图已确认时补一帧透视底图。
    /// 固件收到 type 13 就用它当底图（楼块、侧面、立交桥都是高德画的，不再逐像素挤楼块）；
    /// 没收到、或朝向差超过 8°，固件自己回退到原来的 rasterRows3D，不会变空屏。
    private func maybePushPersp(x: Double, y: Double, bearing: Double, kmh: Int, force: Bool = false) {
        let block = perspGate(x: x, y: y, bearing: bearing, force: force)
        perspBlock = block ?? ""
        guard block == nil, let f = frame else { return }
        perspLastTry = Date(); perspBusy = true
        perspBearing = bearing; perspAnchor = (x, y)
        worker.async { [weak self] in
            var packet: Data?
            var detail = ""
            do {
                let built = try AmapSdkRaster.buildPersp(f, ax: x, ay: y, bearing: bearing, shouldCancel: { false })
                packet = built.packet
                detail = "\(built.shot.width)×\(built.shot.height) \(built.shot.dMin)…\(built.shot.dMax)m z\(String(format: "%.1f", built.shot.zoom))"
            } catch { detail = error.localizedDescription }
            DispatchQueue.main.async {
                guard let self else { return }
                self.perspBusy = false
                guard let packet = packet else {
                    self.perspInfo = "3D 底图：\(detail)"
                    MapPipelineTrace.event("persp-failed", ["reason": detail])
                    return
                }
                self.perspBytes = packet
                self.perspId = (self.perspId + 1) & 0xFF
                self.perspAcked = false
                self.perspInfo = "3D 底图 #\(self.perspId)：\(packet.count / 1024) KB，\(detail)"
                MapPipelineTrace.event("persp-ready", ["id": self.perspId, "bytes": packet.count, "detail": detail])
                self.sendPersp()
            }
        }
    }

    private func sendPersp() {
        guard let b = perspBytes, enabled, Settings.shared.persp3D else { return }
        perspSentAt = Date()
        MapPipelineTrace.event("persp-send", ["id": perspId, "bytes": b.count, "ready": BleLink.shared.isReady])
        BleLink.shared.sendBulk(type: Self.TYPE_PERSP, id: perspId, data: b)
    }

    private func resetPersp() {
        perspBytes = nil; perspAcked = true; perspBusy = false
        perspAnchor = nil; perspBearing = Double.nan; perspInfo = ""
    }

    /// 设置页「立即传一帧」：跳过 7s / 40m / 块中心节流，立刻抢一帧
    func forcePersp() {
        let p = pose ?? (x: 0.0, y: 0.0, kmh: 0)
        perspLastTry = .distantPast
        maybePushPersp(x: p.x, y: p.y, bearing: lastBearing ?? 0, kmh: p.kmh, force: true)
    }

    // MARK: - 路线

    /// 每次收到导航信息时调用，路线变了（新规划 / 偏航重算）就重新发送。
    /// coords、lights 都是高德坐标（纬度, 经度）。还没有坐标系（没定位）时返回 false，稍后再试。
    func onRoute(key: String, coords: [(lat: Double, lng: Double)], lights: [(lat: Double, lng: Double)]) -> Bool {
        guard let f = frame else { return false }
        if key == routeKey { return true }
        if coords.count < 2 { return false }
        routeKey = key
        prepareRoute(key: key, coords: coords)
        routeBuildSerial += 1; let serial = routeBuildSerial
        worker.async {
            var pts = [Double](repeating: 0, count: coords.count * 2)
            for (i, c) in coords.enumerated() { pts[2 * i] = f.x(c.lng); pts[2 * i + 1] = f.y(c.lat) }
            var lp = [Double](repeating: 0, count: lights.count * 2)
            for (i, c) in lights.enumerated() { lp[2 * i] = f.x(c.lng); lp[2 * i + 1] = f.y(c.lat) }
            let b = MapGeo.encodeRoute(frameId: f.id, pts: pts, maxPts: 20000, lights: lp)
            DispatchQueue.main.async {
                guard serial == self.routeBuildSerial else { return }
                self.routeId = (self.routeId + 1) & 0xFF
                self.routeBytes = b
                self.routeAcked = false
                self.routeInfo = "路线 #\(self.routeId)：\(coords.count) 个点、\(lights.count) 个红绿灯 → \(max(1, b.count / 1024)) KB"
                self.sendRoute()
            }
        }
        return true
    }

    private func sendRoute() {
        guard let b = routeBytes, enabled else { return }
        routeSentAt = Date()
        BleLink.shared.sendBulk(type: MapFeeder.TYPE_ROUTE, id: routeId, data: b)
    }

    // MARK: - 原路线

    /// 开始导航时调用：把导入的分享路线发给圆屏画成细线；nil = 不显示
    func setGhost(_ coords: [(lat: Double, lng: Double)]?) {
        ghostCoords = coords
        ghostBytes = nil; ghostAcked = false
        if coords == nil { BleLink.shared.send("#o"); return }
        encodeGhost()
    }

    private func encodeGhost() {
        guard let f = frame, let c = ghostCoords, c.count >= 2 else { return }
        var pts = [Double](repeating: 0, count: c.count * 2)
        for (i, p) in c.enumerated() { pts[2 * i] = f.x(p.lng); pts[2 * i + 1] = f.y(p.lat) }
        ghostBytes = MapGeo.encodeRoute(frameId: f.id, pts: pts, maxPts: 5000)
        ghostId = (ghostId + 1) & 0xFF
        ghostAcked = false
        sendGhost()
    }

    private func sendGhost() {
        guard let b = ghostBytes, enabled else { return }
        ghostSentAt = Date()
        BleLink.shared.sendBulk(type: MapFeeder.TYPE_GHOST, id: ghostId, data: b)
    }

    // MARK: - 确认与重发

    /// 圆屏回复 "K,类型,编号"：这一份收齐了
    func onAck(type: Int, id: Int) {
        // musicA.59：矢量简图发的是要素包（type 1），回执也算底图就绪，否则会反复重算。
        if type == MapFeeder.TYPE_CHUNK, id == netId, Self.THEME_ROADNET.contains(Settings.shared.theme) {
            chunkAcked = true
            confirmedCenter = (chunkCx, chunkCy)
            status = "圆屏已收到路网"
            return
        }
        if type == chunkType && id == chunkId {
            chunkAcked = true; status = "圆屏已收到地图"
            upgradeBase=nil
            confirmedCenter = (chunkCx, chunkCy);lastRoofUpgradeProbe = .distantPast
            DispatchQueue.main.async { [weak self] in self?.flushPreparedMap();self?.tryUpgradeBuildings() }
            MapPipelineTrace.event("map-ack", ["type": type, "id": id, "buildings": chunkBytes.map(RasterPacket.isBuilding) ?? false])
        }
        if type == MapFeeder.TYPE_ROUTE && id == routeId { routeAcked = true }
        if type == MapFeeder.TYPE_GHOST && id == ghostId { ghostAcked = true }
        if type == Self.TYPE_PERSP && id == perspId { perspAcked = true }
    }

    /// 蓝牙重新连上 / 小地图重新打开：圆屏可能重启过，全部重发
    func resendAll() {
        chunkAcked = false; routeAcked = false; ghostAcked = false
        sendChunk(); sendRoute(); sendGhost()
        if perspBytes != nil { perspAcked = false; sendPersp() }
    }

    private func checkResend() {
        let b = BleLink.shared
        if !b.isReady || b.bulkPending { return }
        let chunkWait: TimeInterval = 2.0
        if chunkBytes != nil && !chunkAcked && Date().timeIntervalSince(chunkSentAt) > chunkWait { sendChunk() }
        if routeBytes != nil && !routeAcked && Date().timeIntervalSince(routeSentAt) > RESEND { sendRoute() }
        if ghostBytes != nil && !ghostAcked && Date().timeIntervalSince(ghostSentAt) > RESEND { sendGhost() }
    }

    /// 高德坐标 → 小地图平面坐标（米）；还没有坐标系时返回 nil
    func toFrame(_ lat: Double, _ lng: Double) -> (x: Int, y: Int)? {
        guard let f = frame else { return nil }
        return (Int(f.x(lng).rounded()), Int(f.y(lat).rounded()))
    }

    /// iOS v0.6 首页用的一句话状态
    var shortStatus: String {
        if !enabled { return "已关闭" }
        if chunkAcked { return "圆屏已收到地图" }
        if status.hasPrefix("地图下载失败") { return "下载失败，稍后自动重试" }
        if status.hasPrefix("正在下载") { return "正在下载" }
        if haveChunk { return "已下载，等圆屏收" }
        return "导航时自动下载"
    }

    var infoText: String {
        if !enabled { return "小地图已关闭" }
        return "\(status)\n\(chunkInfo)\(chunkAcked ? "（圆屏已确认）" : "")\n\(routeInfo)\(routeAcked ? "（圆屏已确认）" : "")\n\(offlineStatus)" + (perspInfo.isEmpty ? "" : "\n\(perspInfo)")
    }
}

/// Explicitly enabled, bounded timing diagnostics; no images, payloads or coordinates.
enum MapPipelineTrace {
    private static let gate=NSLock()
    private static let queue=DispatchQueue(label:"motonav.map.trace")
    private static var enabled=false
    private static var buffer=MapTraceBuffer()
    private static var startUptime=0.0
    static func begin() {
        gate.lock();defer{gate.unlock()}
        queue.async {buffer=MapTraceBuffer();startUptime=ProcessInfo.processInfo.systemUptime}
        enabled=true
    }
    static func event(_ name:String,_ fields:[String:Any]=[:]) {
        gate.lock();defer{gate.unlock()}
        guard enabled else {return}
        let uptime=ProcessInfo.processInfo.systemUptime
        queue.async {buffer.append(name:name,fields:fields,seconds:uptime-startUptime)}
    }
    static func packet(_ bytes:Data,type:Int,id:Int,ready:Bool) {
        gate.lock();let recording=enabled;gate.unlock()
        guard recording else {return}
        event("map-send",["type":type,"id":id,"bytes":bytes.count,"ready":ready,
            "decodeValid":RasterPacket.valid(bytes),"buildings":RasterPacket.isBuilding(bytes)])
    }
    static func finish(completion:@escaping(URL?)->Void) {
        gate.lock();defer{gate.unlock()};enabled=false
        queue.async {
            let root=FileManager.default.urls(for:.documentDirectory,in:.userDomainMask)[0].appendingPathComponent("MotoNav-Diagnostics",isDirectory:true)
            let url=root.appendingPathComponent("map-timing-"+UUID().uuidString+".json")
            var report=buffer.document
            report["version"]=APP_VERSION;report["build"]=Bundle.main.object(forInfoDictionaryKey:"CFBundleVersion") ?? ""
            report["note"]="User-started timing session. No keys, cookies, raw packets, images, route names or geographic coordinates."
            do {
                try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true)
                try JSONSerialization.data(withJSONObject:report,options:[.prettyPrinted,.sortedKeys]).write(to:url,options:.atomic)
                DispatchQueue.main.async{completion(url)}
            } catch {DispatchQueue.main.async{completion(nil)}}
        }
    }
}

final class MapTraceReport:ObservableObject {
    static let shared=MapTraceReport()
    @Published private(set) var recording=false
    @Published private(set) var exporting=false
    @Published private(set) var files:[URL]=[]
    @Published private(set) var status="开始模拟前记录，出现底图空白后停止并导出。"
    private var timer:Timer?
    private init() {
        let manager=FileManager.default
        let saved=manager.urls(for:.documentDirectory,in:.userDomainMask)[0].appendingPathComponent("MotoNav-Diagnostics",isDirectory:true)
        let legacy=manager.urls(for:.cachesDirectory,in:.userDomainMask)[0].appendingPathComponent("map-pipeline",isDirectory:true)
        if let previous=MapReportFiles.latestTiming(in:[saved,legacy]) {
            files=[previous];status="已找到上次保存的衔接记录，可直接保存或分享。"
        }
    }
    func begin() {
        guard !recording && !exporting else {return}
        files=[];recording=true;status="正在记录地图衔接，最长 10 分钟。"
        MapPipelineTrace.begin()
        MapPipelineTrace.event("session-start",["maxWrite":BleLink.shared.maxWrite,"ready":BleLink.shared.isReady,"style":Settings.shared.theme])
        timer=Timer.scheduledTimer(withTimeInterval:600,repeats:false){[weak self] _ in self?.finish()}
    }
    func finish() {
        guard recording else {return}
        timer?.invalidate();timer=nil;recording=false;exporting=true;status="正在整理记录…"
        MapPipelineTrace.event("session-stop")
        MapPipelineTrace.finish{url in
            self.exporting=false
            self.files=url.map{[$0]} ?? []
            self.status=url == nil ? "记录保存失败，请重试。":"记录已保存，仅含地图准备、传输与确认的时间和状态。"
        }
    }
}
