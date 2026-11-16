import Foundation

/// An existing encrypted HID connection may outlive the app and its U,1 notifications.
/// Probe after the navigation RX/TX channels are ready; GATT still enforces pairing.
struct BleCapabilityProbe {
    private(set) var attempts = 0
    private var lastAttempt: TimeInterval?
    mutating func next(ready: Bool, supported: Bool, now: TimeInterval) -> Bool {
        // musicA.29：5 次而非 3 次。旧固件只在收到 #GB? 时回一条 CAP,BLD,1，
        // 三次都赶上命令被地图大包挤掉，整场会话就会降级成 legacy 底图（圆屏上没有建筑）。
        guard ready, !supported, attempts < 5 else { return false }
        if let lastAttempt, now - lastAttempt < 2.5 { return false }
        attempts += 1; lastAttempt = now
        return true
    }
    mutating func reset() { attempts = 0; lastAttempt = nil }
}
