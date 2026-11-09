import Foundation
import UIKit
import Combine

/// 中央槽自定义精灵：选图 → 像素化两态多帧 → BLE bulk type 11（sleep + chase）→ ffat。
/// musicA.50：CAP,CSP,1 / CSP,cached,1；本地 Documents 缓存预览帧。
final class CenterSpriteBridge: ObservableObject {
    static let shared = CenterSpriteBridge()

    @Published var subject: CenterSpriteSubject = .person
    @Published private(set) var sourceImage: UIImage?
    @Published private(set) var previewSleep: UIImage?
    @Published private(set) var previewChase: UIImage?
    @Published private(set) var status = ""
    @Published private(set) var transferring = false
    @Published var pushed = false
    @Published var manualCrop: CGRect?
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

    private init() {}

    func selectImage(_ image: UIImage) {
        sourceImage = image
        pushed = false
        skipAutoResend = false
        status = "已选图 · 点「生成动画」"
        rebuild()
    }

    func setSubject(_ s: CenterSpriteSubject) {
        subject = s
        if sourceImage != nil { rebuild() }
    }

    func applyManualCrop(_ norm: CGRect?) {
        manualCrop = norm
        if sourceImage != nil { rebuild() }
    }

    func rebuild() {
        guard let img = sourceImage else { return }
        encodeGeneration += 1
        let gen = encodeGeneration
        transferring = true
        status = "正在抠图并像素化…"
        let sub = subject
        let crop = manualCrop
        DispatchQueue.global(qos: .userInitiated).async {
            let built = CenterSpritePixelizer.build(from: img, subject: sub, manualRect: crop)
            DispatchQueue.main.async {
                guard self.encodeGeneration == gen else { return }
                self.transferring = false
                guard let built else {
                    self.status = sub == .person
                        ? "抠人失败 · 请改用手动框或换图"
                        : "裁切失败 · 请调整选区"
                    return
                }
                self.sleepFrames = built.sleepRGB565
                self.chaseFrames = built.chaseRGB565
                self.previewSleep = built.previewSleep.first
                self.previewChase = built.previewChase.first
                self.pushed = false
                self.skipAutoResend = false
                self.status = "已生成未连接 \(built.sleepRGB565.count) 帧 / 已连接 \(built.chaseRGB565.count) 帧"
            }
        }
    }

    func pushToDevice() {
        guard hasFrames else { status = "请先生成动画"; return }
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
        status = "圆屏已有自定义动画 · 无需重推"
    }

    func clear() {
        encodeGeneration += 1
        sourceImage = nil
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
            status = "已清除自定义动画"
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
        status = quiet ? "已自动重推自定义动画" : "已推送两态动画 · 动画样式已切到自定义"
    }
}
