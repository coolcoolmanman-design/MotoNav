import Foundation
import UIKit
import Combine

/// 中央槽「组合」预制精灵：性别 × 车型 → 两态多帧 → BLE bulk type 11（sleep + chase）→ ffat。
/// musicA.53：取代相册/Vision 上传主路径；CAP,CSP,1 / CSP,cached,1。
final class CenterSpriteBridge: ObservableObject {
    static let shared = CenterSpriteBridge()

    @Published var gender: CenterSpriteGender = {
        let v = UserDefaults.standard.integer(forKey: "csp_gender")
        return CenterSpriteGender(rawValue: v) ?? .male
    }() {
        didSet { UserDefaults.standard.set(gender.rawValue, forKey: "csp_gender") }
    }

    @Published var vehicle: CenterSpriteVehicle = {
        let v = UserDefaults.standard.integer(forKey: "csp_vehicle")
        return CenterSpriteVehicle(rawValue: v) ?? .motorcycle
    }() {
        didSet { UserDefaults.standard.set(vehicle.rawValue, forKey: "csp_vehicle") }
    }

    @Published private(set) var previewSleep: UIImage?
    @Published private(set) var previewChase: UIImage?
    @Published private(set) var status = ""
    @Published private(set) var transferring = false
    @Published var pushed = false
    private var sleepFrames: [[UInt8]] = []
    private var chaseFrames: [[UInt8]] = []
    private var skipAutoResend = false
    private var seq = 0
    private var encodeGeneration = 0

    var hasFrames: Bool { !sleepFrames.isEmpty && !chaseFrames.isEmpty }
    var canSend: Bool {
        BleLink.shared.isReady
            && !NaviBridge.shared.navigating
            && !NaviBridge.shared.cruising
            && !WebMotorSession.shared.running
    }

    private init() {
        rebuild()
    }

    func setGender(_ g: CenterSpriteGender) {
        gender = g
        rebuild()
    }

    func setVehicle(_ v: CenterSpriteVehicle) {
        vehicle = v
        rebuild()
    }

    func rebuild() {
        encodeGeneration += 1
        let gen = encodeGeneration
        transferring = true
        status = "正在生成组合预制…"
        let g = gender
        let v = vehicle
        DispatchQueue.global(qos: .userInitiated).async {
            let built = CenterSpritePixelizer.build(gender: g, vehicle: v)
            DispatchQueue.main.async {
                guard self.encodeGeneration == gen else { return }
                self.transferring = false
                self.sleepFrames = built.sleepRGB565
                self.chaseFrames = built.chaseRGB565
                self.previewSleep = built.previewSleep.first
                self.previewChase = built.previewChase.first
                self.pushed = false
                self.skipAutoResend = false
                self.status = "\(g.title)×\(v.title) · 未连接 \(built.sleepRGB565.count) 帧 / 已连接 \(built.chaseRGB565.count) 帧"
            }
        }
    }

    func pushToDevice() {
        guard hasFrames else {
            status = "请先选择组合"
            rebuild()
            return
        }
        guard canSend else {
            status = BleLink.shared.isReady ? "导航中不可推送" : "请先连接圆屏"
            return
        }
        pushFrames(quiet: false)
    }

    func resendIfPossible() {
        guard canSend, hasFrames, !skipAutoResend else { return }
        pushFrames(quiet: true)
    }

    func noteDeviceCached() {
        skipAutoResend = true
        if hasFrames { pushed = true }
        status = "圆屏已有组合动画 · 无需重推"
    }

    func clear() {
        encodeGeneration += 1
        previewSleep = nil
        previewChase = nil
        sleepFrames = []
        chaseFrames = []
        pushed = false
        skipAutoResend = false
        transferring = false
        status = ""
        BleLink.shared.cancelBulk(type: CenterSpritePacket.bulkType)
        if canSend {
            BleLink.shared.sendBulk(type: CenterSpritePacket.bulkType, id: seq & 0xFF,
                                    data: CenterSpritePacket.clear(role: CenterSpritePacket.roleClearAll))
            seq = (seq + 1) & 0xFF
            status = "已清除组合动画"
        }
        if Settings.shared.vehId == 4 {
            Settings.shared.vehId = 0
        }
    }

    private func pushFrames(quiet: Bool) {
        guard let sleepPkt = CenterSpritePacket.encode(role: CenterSpritePacket.roleSleep, frames: sleepFrames),
              let chasePkt = CenterSpritePacket.encode(role: CenterSpritePacket.roleChase, frames: chaseFrames) else {
            status = "编码失败"; return
        }
        transferring = true
        if !quiet { status = "正在推送未连接循环…" }
        BleLink.shared.sendBulk(type: CenterSpritePacket.bulkType, id: seq & 0xFF, data: sleepPkt)
        seq = (seq + 1) & 0xFF
        BleLink.shared.sendBulk(type: CenterSpritePacket.bulkType, id: seq & 0xFF, data: chasePkt)
        seq = (seq + 1) & 0xFF
        transferring = false
        pushed = true
        skipAutoResend = true
        if Settings.shared.vehId != 4 { Settings.shared.vehId = 4 }
        else { Settings.shared.syncVehicle() }
        status = quiet ? "已自动重推组合动画" : "已推送两态动画 · 动画样式已切到组合"
    }
}
