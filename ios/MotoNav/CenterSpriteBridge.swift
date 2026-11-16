import Foundation
import UIKit
import Combine

/// 中央槽「组合」预制精灵（musicA.55）：16 组合（2 性别 × 8 车型）× 2 态已烧进固件 Flash。
/// App 只发 #CS,<gender>,<vehicle> 两个字节，选择即生效、零 BLE bulk、开机即有。
/// 预览直接读 Assets.xcassets 里同源烘焙的正立 PNG，与圆屏所见一致，
/// 不再走运行时像素化 / CG 翻 Y / 整帧 bob（那些曾造成倒置与随机滚动）。
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
    /// 内置资源没有异步传输；保留该接口供旧 UI / 连接事件使用。
    @Published private(set) var transferring = false
    @Published var pushed = false

    var hasFrames: Bool { true }
    var canSend: Bool { BleLink.shared.isReady }

    /// 与固件 ComboBuiltin.h 的车型索引顺序严格一致。
    private static let assetVehicles = ["sedan", "suv", "supercar", "motorcycle",
                                        "scooter", "sportbike", "bicycle", "foldingBike"]

    private init() { rebuild() }

    func setGender(_ g: CenterSpriteGender) {
        gender = g
        rebuild()
        ensureComboMode()
        syncCombo()
    }

    func setVehicle(_ v: CenterSpriteVehicle) {
        vehicle = v
        rebuild()
        ensureComboMode()
        syncCombo()
    }

    /// 用户直接点组合卡里的选项时，确保圆屏载具模式切到「组合」（vehId=4）。
    private func ensureComboMode() {
        if Settings.shared.vehId != 4 { Settings.shared.vehId = 4 }
    }

    /// 预览与固件同源：Assets 里烘焙好的正立 PNG，无需运行时重绘。
    func rebuild() {
        let g = gender.rawValue == 0 ? "m" : "f"
        let v = Self.assetVehicles[vehicle.rawValue]
        let base = "Combo_\(g)_\(v)"
        previewSleep = UIImage(named: base + "_sleep")
        previewChase = UIImage(named: base + "_wave")
        status = "\(gender.title)×\(vehicle.title) · 固件内置 · 选择即生效"
    }

    /// 选择 / 重连 / 手动「应用到圆屏」都走这里：只发 2 字节索引。
    /// 注意：pushAll() 也调它，但不改 vehId，避免把用户的塞尔达 / GTA 载具选择覆盖掉。
    func syncCombo() {
        guard canSend else {
            status = "\(gender.title)×\(vehicle.title) · 待连接圆屏后自动同步"
            return
        }
        BleLink.shared.send("#CS,\(gender.rawValue),\(vehicle.rawValue)")
        pushed = true
        status = "\(gender.title)×\(vehicle.title) · 已下发，立即生效"
    }

    func comboReply(_ g: Int, _ v: Int) {
        if g >= 0 && v >= 0 {
            pushed = true
            status = "圆屏已应用 · 立即生效"
        } else if g == -1 {
            status = "已切回自定义精灵通路"
        } else {
            status = "保存失败，请重新选择"
        }
    }

    // 兼容既有 UI / 连接事件入口
    func pushToDevice() { ensureComboMode(); syncCombo() }
    func resendIfPossible() { if canSend { syncCombo() } }
    func noteDeviceCached() { if canSend { syncCombo() } }

    /// 「恢复默认组合」：男生 × 摩托车，不再清 ffat、不再切载具。
    func clear() {
        gender = .male
        vehicle = .motorcycle
        rebuild()
        syncCombo()
    }
}
