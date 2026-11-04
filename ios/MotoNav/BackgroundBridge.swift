import Foundation
import UIKit
import Combine

/// 自定义开机背景（musicA.17）：相册选图 → 居中裁方 → 466×466 RGB565 → zlib → BLE bulk type 8。
/// 持久化 = 「开机连 App 自动重推」：App 把图存本地（Documents/bg_custom.png），
/// 圆屏每次认证后广播 CAP,BGR,1 → resendIfPossible() 自动重推；固件掉电不丢（因为每次都重推）。
/// 「恢复默认」发 w=h=0 清除包（#BGD 等价效果），App 本地也删掉存图。
final class BackgroundBridge: ObservableObject {
    static let shared = BackgroundBridge()

    @Published private(set) var previewImage: UIImage?
    @Published private(set) var status = ""
    @Published private(set) var transferring = false
    @Published var pushed = false

    private var seq = 0
    private var encodeGeneration = 0
    private static let storeURL: URL? = {
        let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
        return dir?.appendingPathComponent("bg_custom.png")
    }()

    var hasImage: Bool { previewImage != nil }
    var canSend: Bool {
        BleLink.shared.isReady
            && !NaviBridge.shared.navigating
            && !NaviBridge.shared.cruising
            && !WebMotorSession.shared.running
    }

    private init() {
        loadStoredImage()
    }

    /// 启动时恢复上次选的背景（自动重推的数据源）
    private func loadStoredImage() {
        guard let url = Self.storeURL, let img = UIImage(contentsOfFile: url.path) else { return }
        previewImage = img
        pushed = false
        status = "已保留上次背景 · 连接圆屏后自动重推"
    }

    /// 仅选图预览，不推送（居中裁方，与吧唧同规则）。
    func selectImage(_ image: UIImage) {
        guard let square = BadgeBridge.centerCropSquare(image) else {
            status = "无法裁切图片"; return
        }
        previewImage = square
        pushed = false
        status = "已选择 · 点「推送到圆屏」设为开机背景"
        saveStoredImage(square)
    }

    func clearImage() {
        encodeGeneration += 1
        previewImage = nil
        pushed = false
        transferring = false
        status = ""
        if let url = Self.storeURL { try? FileManager.default.removeItem(at: url) }
        BleLink.shared.cancelBulk(type: BackgroundPacket.bulkType)
        if canSend {
            BleLink.shared.sendBulk(type: BackgroundPacket.bulkType, id: seq & 0xFF, data: BackgroundPacket.clear())
            seq = (seq + 1) & 0xFF
            status = "已恢复默认背景"
        }
    }

    /// 显式推送当前预览到圆屏。
    func pushToDevice() {
        guard let preview = previewImage else {
            status = "请先选择图片"; return
        }
        guard canSend else {
            status = BleLink.shared.isReady ? "导航中不可推送" : "请先连接圆屏"
            return
        }
        pushPreview(preview)
    }

    /// 连接后自动重推（CAP,BGR,1 触发；开机连 App 自动重推的落点）。
    func resendIfPossible() {
        guard canSend, let preview = previewImage else { return }
        pushPreview(preview, quiet: true)
    }

    // MARK: - Wire

    private func pushPreview(_ preview: UIImage, quiet: Bool = false) {
        encodeGeneration += 1
        let gen = encodeGeneration
        transferring = true
        if !quiet { status = "正在编码…" }
        DispatchQueue.global(qos: .userInitiated).async {
            guard let rgba = Self.rgba466(preview) else {
                DispatchQueue.main.async {
                    guard self.encodeGeneration == gen else { return }
                    self.transferring = false
                    self.status = "编码失败"
                }
                return
            }
            var raw = [UInt8](repeating: 0, count: BackgroundPacket.px * BackgroundPacket.px * 2)
            for i in 0..<(BackgroundPacket.px * BackgroundPacket.px) {
                let r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2]
                let v = (UInt16(r) & 0xF8) << 8 | (UInt16(g) & 0xFC) << 3 | (UInt16(b) >> 3)
                raw[i * 2] = UInt8(v & 0xFF)
                raw[i * 2 + 1] = UInt8(v >> 8)
            }
            guard let packet = BackgroundPacket.encode(w: BackgroundPacket.px, h: BackgroundPacket.px, rgb565LE: raw) else {
                DispatchQueue.main.async {
                    guard self.encodeGeneration == gen else { return }
                    self.transferring = false
                    self.status = "编码失败"
                }
                return
            }
            DispatchQueue.main.async {
                guard self.encodeGeneration == gen else { return }
                self.transferring = false
                guard self.canSend else {
                    self.pushed = false
                    self.status = "已就绪 · 连接圆屏后自动重推"
                    return
                }
                BleLink.shared.sendBulk(type: BackgroundPacket.bulkType, id: self.seq & 0xFF, data: packet)
                self.seq = (self.seq + 1) & 0xFF
                self.pushed = true
                self.status = quiet ? "已自动重推到圆屏" : "已推送到圆屏 · 待机页即刻生效"
            }
        }
    }

    private func saveStoredImage(_ image: UIImage) {
        guard let url = Self.storeURL,
              let data = image.pngData() else { return }
        try? data.write(to: url)
    }

    /// 缩放/绘制到 466×466 RGBA（与 BadgeBridge.rgba466 同实现；那边是 private）。
    private static func rgba466(_ image: UIImage) -> [UInt8]? {
        let px = BackgroundPacket.px
        let side = CGFloat(px)
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        format.opaque = true
        let renderer = UIGraphicsImageRenderer(size: CGSize(width: side, height: side), format: format)
        let drawn = renderer.image { _ in
            image.draw(in: CGRect(x: 0, y: 0, width: side, height: side))
        }
        guard let cg = drawn.cgImage else { return nil }
        var out = [UInt8](repeating: 0, count: px * px * 4)
        guard let ctx = CGContext(data: &out, width: px, height: px, bitsPerComponent: 8,
                                  bytesPerRow: px * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.interpolationQuality = .high
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: side, height: side))
        return out
    }
}
