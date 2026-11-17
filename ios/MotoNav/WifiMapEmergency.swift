import Foundation

/// BLE 主 / WiFi 应急：高速或即将空白时才开 SoftAP，避免常驻耗电。
enum WifiMapEmergencyPolicy {
    static let enterKmh = 70
    static let exitKmh = 45
    static let holdSeconds: TimeInterval = 25
    static let satelliteEnterKmh = 50

    static func shouldEngage(kmh: Int, blankRisk: Bool, satellite: Bool, currentlyActive: Bool,
                             transferHold: Bool = false) -> Bool {
        // 传图未完 / 未确认时禁止关热点，否则 TCP 半路失败再 BLE 兜底会空白数十秒
        if transferHold { return true }
        if blankRisk { return true }
        let enter = satellite ? satelliteEnterKmh : enterKmh
        if kmh >= enter { return true }
        if currentlyActive && kmh >= exitKmh { return true }
        return false
    }
}
