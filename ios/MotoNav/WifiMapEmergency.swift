import Foundation

/// BLE 主 / WiFi 应急：高速或即将空白时才开 SoftAP，避免常驻耗电。
enum WifiMapEmergencyPolicy {
    static let enterKmh = 70
    static let exitKmh = 45
    static let holdSeconds: TimeInterval = 25
    static let satelliteEnterKmh = 50

    // 0.16.2.128: 关闭应急自动开 WiFi。导航中首帧 #WIFI,1 会触发固件主动重启一次
    // (musicA.117 preinit)，且本板 BLE + SoftAP 不可兼得；底图全程走 BLE。
    // 手动开关 WifiMapLink.applySetting(true) 仍走 requestSoftAP，不受此处影响。
    static let emergencyWiFiEnabled = false

    static func shouldEngage(kmh: Int, blankRisk: Bool, satellite: Bool, currentlyActive: Bool,
                             transferHold: Bool = false) -> Bool {
        guard emergencyWiFiEnabled else { return false }
        // 传图未完 / 未确认时禁止关热点，否则 TCP 半路失败再 BLE 兜底会空白数十秒
        if transferHold { return true }
        if blankRisk { return true }
        let enter = satellite ? satelliteEnterKmh : enterKmh
        if kmh >= enter { return true }
        if currentlyActive && kmh >= exitKmh { return true }
        return false
    }
}
