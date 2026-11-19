import Foundation
import Network
import NetworkExtension
import UIKit

/// 试验：圆屏 SoftAP + TCP:7443 整包底图（避免 HTTP/String 截断二进制）。
/// musicA.95 自 git 历史移植（090aa9e 最终态，mark→event 适配当前诊断 API）。
final class WifiMapLink: ObservableObject {
    static let shared = WifiMapLink()
    /// 0.16.2.101 / musicA.111：SoftAP 地图加速（可选；关则纯 BLE）。
    /// 固件暖 SoftAP（BLE 前 hidden AP 不 stop）+ 协议 host 单测通过后再发 Draft。
    static let featureEnabled = true

    enum PutResult: Equatable { case sent, failed, busy, superseded }

    @Published private(set) var softApOn = false
    @Published private(set) var reachable = false
    @Published private(set) var status = "未启用"
    @Published private(set) var ssid = ""
    @Published private(set) var psk = ""
    @Published private(set) var host = "192.168.4.1"
    @Published private(set) var tcpPort: UInt16 = 7443
    @Published private(set) var transferActive = false
    @Published private(set) var lastPutOK = false
    @Published private(set) var lastPutMs = 0
    @Published private(set) var lastPutKB = 0
    /// 策略 A：高速/空白风险临时开 SoftAP（手动「WiFi 地图加速」仍可常开）
    @Published private(set) var emergencyActive = false
    /// TCP 连续失败后短时只走 BLE，避免与 BLE 分包互相 restart
    @Published private(set) var bleOnlyActive = false

    private var probeTimer: Timer?
    private var openRetryTimer: Timer?
    private var openRetryLeft = 0
    private var sending = false
    private var lastPutAt = Date.distantPast
    private var failStreak = 0
    private var emergencyHoldUntil = Date.distantPast
    private var bleOnlyUntil = Date.distantPast
    /// 在途时新块 latest-wins，避免 busy 直接丢包（实车空白主因之一）
    private var pendingPut: (id: Int, data: Data, completion: (PutResult) -> Void)?
    private var activePutId: Int?
    private var putGeneration = 0
    private var transferBgTask: UIBackgroundTaskIdentifier = .invalid
    private let probeSession: URLSession = {
        let c = URLSessionConfiguration.ephemeral
        c.timeoutIntervalForRequest = 2
        c.waitsForConnectivity = false
        c.allowsCellularAccess = false
        return URLSession(configuration: c)
    }()

    var shouldPreferWifi: Bool {
        guard Self.featureEnabled else { return false }
        clearBleOnlyIfNeeded()
        if Date() < bleOnlyUntil { return false }
        // Require reachable so we don't burn time on TCP timeouts while not on SoftAP
        return (Settings.shared.wifiMapBoost || emergencyActive) && softApOn && reachable && !host.isEmpty
    }

    var canSend: Bool { shouldPreferWifi }
    var isSending: Bool { sending }

    /// 锁屏后给 TCP 整包传输续一小段后台时间。SoftAP 本地包通常远早于
    /// 后台配额完成；iOS 仍可能在超时后挂起 App，回前台会补发缓存块。
    private func beginTransferBackgroundTask() {
        if Thread.isMainThread { startTransferBackgroundTask(); return }
        DispatchQueue.main.async { [weak self] in self?.startTransferBackgroundTask() }
    }
    private func startTransferBackgroundTask() {
        guard transferBgTask == .invalid else { return }
        transferBgTask = UIApplication.shared.beginBackgroundTask(withName: "MotoNav WiFi map transfer") { [weak self] in
            self?.endTransferBackgroundTask()
        }
    }
    private func endTransferBackgroundTask() {
        guard transferBgTask != .invalid else { return }
        let task = transferBgTask
        transferBgTask = .invalid
        if Thread.isMainThread {
            UIApplication.shared.endBackgroundTask(task)
        } else {
            DispatchQueue.main.async { UIApplication.shared.endBackgroundTask(task) }
        }
    }

    /// WiFi TCP 失败或已开始 BLE 回退：清排队、作废在途 TCP，冷却期内不再抢 BLE。
    func enterBleOnly(reason: String, seconds: TimeInterval = 45) {
        putGeneration += 1
        bleOnlyUntil = Date().addingTimeInterval(seconds)
        bleOnlyActive = true
        let orphan = pendingPut
        pendingPut = nil
        sending = false
        activePutId = nil
        transferActive = false
        endTransferBackgroundTask()
        MapPipelineTrace.event("wifi-ble-only", ["reason": reason, "sec": Int(seconds), "failStreak": failStreak])
        orphan?.completion(.superseded)
    }

    func clearBleOnlyIfNeeded() {
        if bleOnlyActive, Date() >= bleOnlyUntil {
            bleOnlyActive = false
            MapPipelineTrace.event("wifi-ble-only-end")
        }
    }

    func applySetting(_ on: Bool) {
        guard Self.featureEnabled else { return }
        if on {
            if emergencyActive { emergencyActive = false }
            requestSoftAP(reason: "手动开加速", retries: 6)
        } else {
            cancelOpenRetries()
            softApOn = false
            reachable = false
            transferActive = false
            failStreak = 0
            emergencyActive = false
            emergencyHoldUntil = .distantPast
            bleOnlyUntil = .distantPast
            bleOnlyActive = false
            pendingPut = nil
            putGeneration += 1
            sending = false
            endTransferBackgroundTask()
            stopProbe()
            status = "已关闭（底图走蓝牙；≥\(WifiMapEmergencyPolicy.enterKmh) km/h 仍可应急开热点）"
            BleLink.shared.sendOnce("#WIFI,0")
            if !ssid.isEmpty {
                NEHotspotConfigurationManager.shared.removeConfiguration(forSSID: ssid)
            }
        }
    }

    /// 只发 #WIFI,1，等圆屏主动回 WIFI,1,…；勿立刻 #WIFI?（重试中会被误判成永久失败）。
    private func requestSoftAP(reason: String, retries: Int) {
        cancelOpenRetries()
        openRetryLeft = max(0, retries)
        status = "正在请求圆屏开热点…"
        MapPipelineTrace.event("wifi-softap-request", ["reason": reason, "retries": retries])
        BleLink.shared.sendOnce("#WIFI,1")
        // 暖 SoftAP 路径通常很快回 WIFI,1；仍勿立刻 #WIFI?（重试中无终态）。
        // 总等待约 6×3.5s，对齐固件最多 8 次退避。
        openRetryTimer = Timer.scheduledTimer(withTimeInterval: 3.5, repeats: true) { [weak self] _ in
            guard let self else { return }
            if self.softApOn {
                self.cancelOpenRetries()
                return
            }
            if self.openRetryLeft <= 0 {
                self.cancelOpenRetries()
                if !self.softApOn {
                    self.status = "无法打开圆屏热点"
                    BleLink.shared.sendOnce("#WIFI?")
                }
                return
            }
            self.openRetryLeft -= 1
            self.status = "圆屏热点重试中（\(self.openRetryLeft)）…"
            BleLink.shared.sendOnce("#WIFI,1")
        }
    }

    private func cancelOpenRetries() {
        openRetryTimer?.invalidate()
        openRetryTimer = nil
        openRetryLeft = 0
    }

    /// 导航中评估是否临时开 SoftAP。手动 wifiMapBoost 开启时不介入。
    /// `transferHold`：底图未确认或 TCP 在途时保持热点，防止半路 #WIFI,0。
    func evaluateEmergency(kmh: Int, blankRisk: Bool, satellite: Bool, navigating: Bool, transferHold: Bool = false) {
        guard Self.featureEnabled else { return }
        if Settings.shared.wifiMapBoost {
            if emergencyActive { emergencyActive = false }
            return
        }
        guard navigating, Settings.shared.mapOn, BleLink.shared.isReady else {
            // 导航结束也等当前 TCP 结束再关，避免失败后还要 BLE 兜旧包
            if emergencyActive, !sending, pendingPut == nil { clearEmergency(reason: "导航结束") }
            return
        }
        let hold = transferHold || sending || pendingPut != nil
        let want = WifiMapEmergencyPolicy.shouldEngage(
            kmh: kmh, blankRisk: blankRisk, satellite: satellite,
            currentlyActive: emergencyActive, transferHold: hold)
        if want {
            emergencyHoldUntil = Date().addingTimeInterval(WifiMapEmergencyPolicy.holdSeconds)
            guard !emergencyActive else { return }
            emergencyActive = true
            status = blankRisk
                ? "空白风险 · 应急开 WiFi…"
                : "高速 ≥\(WifiMapEmergencyPolicy.enterKmh) km/h · 应急开 WiFi…"
            MapPipelineTrace.event("wifi-emergency-on", [
                "kmh": kmh, "blankRisk": blankRisk, "sat": satellite, "transferHold": hold
            ])
            requestSoftAP(reason: "应急", retries: 2)
        } else if emergencyActive, Date() >= emergencyHoldUntil, !hold {
            clearEmergency(reason: "车速回落")
        }
    }

    private func clearEmergency(reason: String) {
        let hadPending = pendingPut != nil || sending
        putGeneration += 1 // 作废在途 TCP 回调，避免关热点后还 BLE 回退旧包
        let orphan = pendingPut
        pendingPut = nil
        emergencyActive = false
        emergencyHoldUntil = .distantPast
        softApOn = false
        reachable = false
        transferActive = false
        sending = false
        activePutId = nil
        endTransferBackgroundTask()
        cancelOpenRetries()
        stopProbe()
        status = "应急 WiFi 已关（\(reason)）· 底图走蓝牙"
        MapPipelineTrace.event("wifi-emergency-off", ["reason": reason, "hadPending": hadPending])
        BleLink.shared.sendOnce("#WIFI,0")
        if !ssid.isEmpty {
            NEHotspotConfigurationManager.shared.removeConfiguration(forSSID: ssid)
        }
        orphan?.completion(.failed)
        // 热点关掉后立刻让未确认底图走 BLE，勿干等 TCP 超时
        MapFeeder.shared.onWifiPathLost(reason: reason)
    }

    /// WIFI,1,ssid,psk,ip[,port] 或 WIFI,0[,err]
    func onNotify(_ parts: [String]) {
        guard Self.featureEnabled else { return }
        guard parts.first == "WIFI", parts.count >= 2 else { return }
        if parts[1] == "0" {
            softApOn = false
            reachable = false
            transferActive = false
            endTransferBackgroundTask()
            stopProbe()
            let isErr = parts.count >= 3 && parts[2] == "err"
            if isErr {
                // 固件侧仍可能在重试；若 App 还有重试配额，继续发 #WIFI,1，勿立刻宣判
                if openRetryTimer != nil, openRetryLeft > 0 {
                    status = "圆屏热点打开中…"
                    return
                }
                cancelOpenRetries()
                status = "无法打开圆屏热点"
            } else {
                cancelOpenRetries()
                status = "圆屏 WiFi 已关"
            }
            return
        }
        guard parts.count >= 5 else { return }
        cancelOpenRetries()
        softApOn = true
        ssid = parts[2]
        psk = parts[3]
        host = parts[4]
        if parts.count >= 6, let p = UInt16(parts[5]), p > 0 { tcpPort = p }
        else { tcpPort = 7443 }
        status = "圆屏热点 \(ssid)，正在加入（TCP \(tcpPort)）…"
        joinSoftAP()
        startProbe()
    }

    func pushIfNeeded() {
        guard Self.featureEnabled, Settings.shared.wifiMapBoost else { return }
        requestSoftAP(reason: "重连补发", retries: 2)
    }

    private func joinSoftAP() {
        guard !ssid.isEmpty, !psk.isEmpty else { return }
        let cfg = NEHotspotConfiguration(ssid: ssid, passphrase: psk, isWEP: false)
        cfg.joinOnce = true
        NEHotspotConfigurationManager.shared.apply(cfg) { [weak self] err in
            DispatchQueue.main.async {
                guard let self else { return }
                if let err = err as NSError? {
                    if err.code == NEHotspotConfigurationError.alreadyAssociated.rawValue {
                        self.status = "已在 \(self.ssid)，探测连通…"
                    } else if err.code == NEHotspotConfigurationError.userDenied.rawValue {
                        self.status = "系统拒绝加入「\(self.ssid)」；请在弹窗点加入"
                    } else {
                        // 常见：缺 Hotspot Configuration 能力 / 用户手动 Wi‑Fi
                        // 脱敏：状态栏不展示 SoftAP 明文密码（PSK 仅用于 NEHotspot 自动加入）
                        self.status = "请在系统设置手动加入 WiFi「\(self.ssid)」（密码见圆屏提示，不在此展示）"
                        MapPipelineTrace.event("wifi-hotspot-join-fail", [
                            "code": err.code, "domain": err.domain
                        ])
                    }
                } else {
                    self.status = "已请求加入 \(self.ssid)，探测连通…"
                }
                self.probeOnce()
            }
        }
    }

    private func startProbe() {
        stopProbe()
        probeTimer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { [weak self] _ in
            self?.probeOnce()
        }
        probeOnce()
    }

    private func stopProbe() {
        probeTimer?.invalidate()
        probeTimer = nil
    }

    private func probeOnce() {
        guard softApOn, let url = URL(string: "http://\(host)/") else { return }
        var req = URLRequest(url: url, timeoutInterval: 2)
        req.httpMethod = "GET"
        req.allowsCellularAccess = false
        probeSession.dataTask(with: req) { [weak self] _, resp, _ in
            let ok = (resp as? HTTPURLResponse).map { (200..<500).contains($0.statusCode) } ?? false
            DispatchQueue.main.async {
                guard let self else { return }
                let becameReachable = ok && !self.reachable
                self.reachable = ok
                if !self.transferActive && !self.sending {
                    self.status = ok
                        ? (self.emergencyActive
                           ? "应急 WiFi 已通 · TCP \(self.tcpPort)"
                           : "WiFi 已通 · TCP \(self.tcpPort) 整包传图")
                        : "热点已开，等待手机加入「\(self.ssid)」"
                }
                // SoftAP 刚通：立刻把未确认底图从 BLE 分包切到 TCP 整包
                if becameReachable, Settings.shared.wifiMapBoost || self.emergencyActive {
                    MapFeeder.shared.onWifiMapReady()
                }
            }
        }.resume()
    }

    /// TCP 整包。在途时挂起最新一块（latest-wins），完成后自动发；失败再由上层 BLE。
    func putRaster(id: Int, data: Data, completion: @escaping (PutResult) -> Void) {
        guard shouldPreferWifi else {
            completion(.failed)
            return
        }
        if sending {
            if let old = pendingPut {
                MapPipelineTrace.event("wifi-put-supersede", [
                    "oldId": old.id, "newId": id, "bytes": data.count
                ])
                old.completion(.superseded) // 勿当 failed，否则会误触发 BLE 并与新排队打架
            }
            pendingPut = (id, data, completion)
            MapPipelineTrace.event("wifi-put-queued", [
                "id": id, "bytes": data.count, "activeId": activePutId ?? -1
            ])
            return
        }
        startPut(id: id, data: data, completion: completion)
    }

    private func startPut(id: Int, data: Data, completion: @escaping (PutResult) -> Void) {
        beginTransferBackgroundTask()
        let generation = putGeneration
        sending = true
        transferActive = true
        activePutId = id
        let kb = max(1, data.count / 1024)
        status = String(format: "WiFi TCP 发送中 #%d · %d KB…", id, kb)
        MapPipelineTrace.event("wifi-put-start", [
            "id": id, "bytes": data.count, "emergency": emergencyActive
        ])
        sendTcp(id: id, data: data, attempt: 1, generation: generation, completion: completion)
    }

    private func finishPut(_ result: PutResult, id: Int, generation: Int, completion: @escaping (PutResult) -> Void) {
        guard generation == putGeneration else { endTransferBackgroundTask(); return }
        sending = false
        activePutId = nil
        if result != .sent { transferActive = false }
        if result == .sent {
            failStreak = 0
            completion(.sent)
            // 仅成功后才 dequeue；失败交给 BLE-only，禁止立刻再开一条 WiFi 与 BLE 互抢
            if let next = pendingPut {
                pendingPut = nil
                guard shouldPreferWifi else {
                    next.completion(.superseded)
                    endTransferBackgroundTask()
                    return
                }
                MapPipelineTrace.event("wifi-put-dequeue", [
                    "id": next.id, "bytes": next.data.count, "afterId": id
                ])
                startPut(id: next.id, data: next.data, completion: next.completion)
                return
            }
            endTransferBackgroundTask()
            return
        }
        failStreak += 1
        let orphan = pendingPut
        pendingPut = nil
        completion(.failed)
        orphan?.completion(.superseded)
        endTransferBackgroundTask()
    }

    private func sendTcp(id: Int, data: Data, attempt: Int, generation: Int, completion: @escaping (PutResult) -> Void) {
        let kb = max(1, data.count / 1024)
        var frame = Data()
        frame.append(contentsOf: [0x4D, 0x4E, 0x01, 3, UInt8(id & 0xff)])
        var be = UInt32(data.count).bigEndian
        withUnsafeBytes(of: &be) { frame.append(contentsOf: $0) }
        frame.append(0) // reserved
        frame.append(data)

        let params = NWParameters.tcp
        params.requiredInterfaceType = .wifi
        params.allowLocalEndpointReuse = true
        let conn = NWConnection(host: NWEndpoint.Host(host), port: NWEndpoint.Port(rawValue: tcpPort)!, using: params)
        let t0 = Date()
        let queue = DispatchQueue(label: "motonav.wifi.map")
        var finished = false
        // 应急路径缩短超时：高速下 20s×3 会空白上百米
        let timeoutSec: TimeInterval = emergencyActive ? 8 : 12
        let maxAttempts = emergencyActive ? 2 : 3

        func finish(_ result: PutResult, detail: String) {
            guard !finished else { return }
            finished = true
            conn.cancel()
            let ms = Int(Date().timeIntervalSince(t0) * 1000)
            DispatchQueue.main.async {
                guard generation == self.putGeneration else { return }
                self.lastPutAt = Date()
                self.lastPutMs = ms
                self.lastPutKB = kb
                self.lastPutOK = (result == .sent)
                if result == .sent {
                    self.reachable = true
                    self.transferActive = true
                    self.status = String(format: "WiFi 已发 #%d · %d KB · %d ms（%@）· 等确认", id, kb, ms, detail)
                    self.finishPut(.sent, id: id, generation: generation, completion: completion)
                } else if attempt < maxAttempts, self.shouldPreferWifi, self.pendingPut == nil {
                    // 有更新块排队则不再重试旧包
                    self.status = String(format: "WiFi 重试 %d/%d：%@", attempt + 1, maxAttempts, detail)
                    self.sending = true
                    self.transferActive = true
                    self.activePutId = id
                    self.sendTcp(id: id, data: data, attempt: attempt + 1, generation: generation, completion: completion)
                } else {
                    self.status = "WiFi 失败(\(detail))，回退蓝牙"
                    self.finishPut(.failed, id: id, generation: generation, completion: completion)
                }
            }
        }

        conn.stateUpdateHandler = { state in
            switch state {
            case .ready:
                conn.send(content: frame, completion: .contentProcessed { err in
                    if let err {
                        finish(.failed, detail: err.localizedDescription)
                        return
                    }
                    conn.receive(minimumIncompleteLength: 1, maximumLength: 64) { content, _, _, err in
                        if let err {
                            finish(.failed, detail: err.localizedDescription)
                            return
                        }
                        let text = content.flatMap { String(data: $0, encoding: .utf8) }?
                            .trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
                        if text.hasPrefix("K,") {
                            finish(.sent, detail: text)
                        } else {
                            finish(.failed, detail: text.isEmpty ? "no-ack" : text)
                        }
                    }
                })
            case .failed(let err):
                finish(.failed, detail: err.localizedDescription)
            case .cancelled:
                break
            default:
                break
            }
        }
        conn.start(queue: queue)
        queue.asyncAfter(deadline: .now() + timeoutSec) {
            finish(.failed, detail: "timeout")
        }
    }

    func markChunkAcked() {
        transferActive = false
        failStreak = 0
        // 本块已确认，允许下一块再试 WiFi（冷却到点后）
        if bleOnlyActive, Date() >= bleOnlyUntil {
            bleOnlyActive = false
        }
        if softApOn {
            status = reachable
                ? "WiFi 已通 · TCP \(tcpPort) 整包传图"
                : "热点已开，等待手机加入「\(ssid)」"
        }
    }

    var recentlySentViaWifi: Bool {
        lastPutOK && Date().timeIntervalSince(lastPutAt) < 15
    }
}
