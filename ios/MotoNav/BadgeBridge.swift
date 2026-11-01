import Foundation
import UIKit
import Combine

/// 电子吧唧静图：相册 → 短边居中裁方 → 466×466 RGB565 → zlib → BLE bulk type 7。
/// 导航 / 巡航 / 网页模拟导航中禁止发送；开始导航时 `#BM,0` 并清空 type 7 队列。
final class BadgeBridge: ObservableObject {
    static let shared = BadgeBridge()

    @Published private(set) var previewImage: UIImage?
    @Published private(set) var pushed = false
    @Published private(set) var status = ""
    @Published private(set) var transferring = false

    private var seq = 0
    private var lastPacket: Data?
    private var encodeGeneration = 0

    var canSend: Bool {
        BleLink.shared.isReady
            && !NaviBridge.shared.navigating
            && !NaviBridge.shared.cruising
            && !WebMotorSession.shared.running
    }

    /// 用户选图后：裁方预览 + 推送到设备（若可发）。
    func applyPicked(_ image: UIImage) {
        guard let square = Self.centerCropSquare(image) else {
            status = "无法裁切图片"; return
        }
        previewImage = square
        pushed = false
        status = "正在编码…"
        encodeAndSend(square)
    }

    func resendIfPossible() {
        guard canSend, let packet = lastPacket else { return }
        send(packet)
        pushed = true
        status = "已推送到圆屏"
    }

    /// 导航 / 巡航开始：退出设备吧唧模式并取消未发完的 type 7。
    func forceExitOnNavigation() {
        encodeGeneration += 1
        transferring = false
        BleLink.shared.cancelBulk(type: BadgePacket.bulkType)
        if BleLink.shared.isReady {
            BleLink.shared.send("#BM,0")
        }
        if pushed { status = "导航中已退出吧唧" }
    }

    func reset() {
        encodeGeneration += 1
        previewImage = nil
        pushed = false
        lastPacket = nil
        transferring = false
        status = ""
        BleLink.shared.cancelBulk(type: BadgePacket.bulkType)
    }

    private func encodeAndSend(_ square: UIImage) {
        encodeGeneration += 1
        let gen = encodeGeneration
        transferring = true
        DispatchQueue.global(qos: .userInitiated).async {
            guard let rgba = Self.rgba466(square) else {
                DispatchQueue.main.async {
                    guard self.encodeGeneration == gen else { return }
                    self.transferring = false
                    self.status = "编码失败"
                }
                return
            }
            var raw = [UInt8](repeating: 0, count: BadgePacket.px * BadgePacket.px * 2)
            for i in 0..<(BadgePacket.px * BadgePacket.px) {
                let r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2]
                let v = (UInt16(r) & 0xF8) << 8 | (UInt16(g) & 0xFC) << 3 | (UInt16(b) >> 3)
                raw[i * 2] = UInt8(v & 0xFF)
                raw[i * 2 + 1] = UInt8(v >> 8)
            }
            let packet = BadgePacket.encode(w: BadgePacket.px, h: BadgePacket.px, rgb565LE: raw)
            DispatchQueue.main.async {
                guard self.encodeGeneration == gen else { return }
                self.transferring = false
                guard let packet else { self.status = "编码失败"; return }
                self.lastPacket = packet
                guard self.canSend else {
                    self.pushed = false
                    self.status = "已就绪 · 未导航时自动推送"
                    return
                }
                self.send(packet)
                self.pushed = true
                self.status = "已推送到圆屏"
            }
        }
    }

    private func send(_ data: Data) {
        BleLink.shared.sendBulk(type: BadgePacket.bulkType, id: seq & 0xFF, data: data)
        seq = (seq + 1) & 0xFF
    }

    /// 短边居中裁成正方形（保留 UIImage，供预览与设备同裁）。
    static func centerCropSquare(_ image: UIImage) -> UIImage? {
        let scale = image.scale
        let s = min(image.size.width, image.size.height)
        guard s > 0, let cg = image.cgImage else { return nil }
        let crop = CGRect(x: (image.size.width - s) / 2 * scale,
                          y: (image.size.height - s) / 2 * scale,
                          width: s * scale, height: s * scale)
        guard let cut = cg.cropping(to: crop) else { return nil }
        return UIImage(cgImage: cut, scale: scale, orientation: image.imageOrientation)
    }

    private static func rgba466(_ image: UIImage) -> [UInt8]? {
        let px = BadgePacket.px
        let side = CGFloat(px)
        guard let cg = image.cgImage else { return nil }
        var out = [UInt8](repeating: 0, count: px * px * 4)
        guard let ctx = CGContext(data: &out, width: px, height: px, bitsPerComponent: 8,
                                  bytesPerRow: px * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.interpolationQuality = .high
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: side, height: side))
        return out
    }
}
