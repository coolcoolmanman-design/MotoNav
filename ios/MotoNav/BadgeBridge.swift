import Foundation
import UIKit
import Combine

enum BadgeCropMode: Int, CaseIterable {
    case autoCenter = 0
    case manualRegion = 1

    var title: String {
        switch self {
        case .autoCenter: return "自动居中"
        case .manualRegion: return "自选区域"
        }
    }
}

/// 电子吧唧单图：相册选图 ≠ 上传；自选自由宽高比选区 → 居中裁方 → 466×466 RGB565 → zlib → BLE bulk type 7。
/// 协议仍兼容固件 opt.12 ver=2 多槽，但 App/UI 只发 slot 0 / count 1（或 ver=1）。
/// 导航 / 巡航 / 网页模拟导航中禁止发送；开始导航时 `#BM,0` 并清空 type 7 队列。
final class BadgeBridge: ObservableObject {
    static let shared = BadgeBridge()

    @Published private(set) var sourceImage: UIImage?
    @Published private(set) var previewImage: UIImage?
    @Published private(set) var cropMode: BadgeCropMode = .autoCenter
    /// 归一化自由选区（相对原图像素 0…1）；nil = 自动短边居中。manual 可为非正方形。
    @Published private(set) var normalizedCrop: CGRect?
    @Published private(set) var pushed = false
    @Published private(set) var status = ""
    @Published private(set) var transferring = false

    private var seq = 0
    private var encodeGeneration = 0

    var canSend: Bool {
        BleLink.shared.isReady
            && !NaviBridge.shared.navigating
            && !NaviBridge.shared.cruising
            && !WebMotorSession.shared.running
    }

    var hasImage: Bool { previewImage != nil }
    /// 兼容旧 UI 字段（单图）。
    var count: Int { hasImage ? 1 : 0 }
    var selectedIndex: Int { 0 }

    /// 仅选图预览，不推送。
    func selectImage(_ image: UIImage) {
        guard let square = Self.centerCropSquare(image) else {
            status = "无法裁切图片"; return
        }
        sourceImage = image
        cropMode = .autoCenter
        normalizedCrop = nil
        previewImage = square
        pushed = false
        status = "已选择 · 点「上传图片」推送到圆屏"
    }

    /// 更新裁切（编辑器「完成」）；不自动上传。
    func applyCrop(mode: BadgeCropMode, normalizedCrop: CGRect?) {
        guard let source = sourceImage else { return }
        self.cropMode = mode
        self.normalizedCrop = (mode == .manualRegion) ? normalizedCrop : nil
        guard let preview = Self.cropToDeviceSquare(source, mode: mode, normalized: self.normalizedCrop) else {
            status = "无法裁切图片"; return
        }
        previewImage = preview
        pushed = false
        status = "已裁切 · 点「上传图片」推送到圆屏"
    }

    func clearImage() {
        encodeGeneration += 1
        sourceImage = nil
        previewImage = nil
        cropMode = .autoCenter
        normalizedCrop = nil
        pushed = false
        transferring = false
        status = ""
        BleLink.shared.cancelBulk(type: BadgePacket.bulkType)
        if canSend {
            BleLink.shared.sendBulk(type: BadgePacket.bulkType, id: seq & 0xFF, data: BadgePacket.clear())
            seq = (seq + 1) & 0xFF
        }
    }

    /// 显式上传当前预览到圆屏（单包，避免 BleBulkWriteQueue.replace 清掉前序 type7）。
    func uploadToDevice() {
        guard let preview = previewImage else {
            status = "请先选择图片"; return
        }
        guard canSend else {
            status = BleLink.shared.isReady ? "导航中不可上传" : "请先连接圆屏"
            return
        }
        pushPreview(preview)
    }

    func resendIfPossible() {
        guard canSend, let preview = previewImage else { return }
        pushPreview(preview)
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
        sourceImage = nil
        previewImage = nil
        cropMode = .autoCenter
        normalizedCrop = nil
        pushed = false
        transferring = false
        status = ""
        BleLink.shared.cancelBulk(type: BadgePacket.bulkType)
    }

    // MARK: - Wire

    private func pushPreview(_ preview: UIImage) {
        encodeGeneration += 1
        let gen = encodeGeneration
        transferring = true
        status = "正在编码…"
        DispatchQueue.global(qos: .userInitiated).async {
            guard let rgba = Self.rgba466(preview) else {
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
            // ver=1 单包：与 opt.11/opt.12 均兼容；且只占一个 bulk id，不被 replace 清掉。
            guard let packet = BadgePacket.encodeV1(w: BadgePacket.px, h: BadgePacket.px, rgb565LE: raw) else {
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
                    self.status = "已就绪 · 连接后点「上传图片」"
                    return
                }
                BleLink.shared.sendBulk(type: BadgePacket.bulkType, id: self.seq & 0xFF, data: packet)
                self.seq = (self.seq + 1) & 0xFF
                self.pushed = true
                self.status = "已推送到圆屏 · 主屏上划查看"
            }
        }
    }

    // MARK: - Crop helpers

    static func centerCropSquare(_ image: UIImage) -> UIImage? {
        cropToDeviceSquare(image, mode: .autoCenter, normalized: nil)
    }

    /// 自由选区 → 在选区内居中裁方 → 再由调用方缩到 466（此处先出方图）。
    /// `normalized`：相对 `UIImage.size` 的自由矩形（可非正方形）；manual 模式必填。
    static func cropToDeviceSquare(_ image: UIImage, mode: BadgeCropMode, normalized: CGRect?) -> UIImage? {
        let scale = image.scale
        guard let cg = image.cgImage else { return nil }
        let iw = image.size.width, ih = image.size.height
        guard iw > 0, ih > 0 else { return nil }

        let region: CGRect
        if mode == .manualRegion, let n = normalized, n.width > 0, n.height > 0 {
            var x = max(0, min(n.origin.x, 1))
            var y = max(0, min(n.origin.y, 1))
            var w = max(0.05, min(n.width, 1 - x))
            var h = max(0.05, min(n.height, 1 - y))
            if x + w > 1 { w = 1 - x }
            if y + h > 1 { h = 1 - y }
            region = CGRect(x: x * iw * scale, y: y * ih * scale,
                            width: w * iw * scale, height: h * ih * scale)
        } else {
            let s = min(iw, ih)
            region = CGRect(x: (iw - s) / 2 * scale, y: (ih - s) / 2 * scale,
                            width: s * scale, height: s * scale)
        }

        // 选区内居中裁方，再交给 rgba466 缩放到 466×466
        let side = min(region.width, region.height)
        guard side > 1 else { return nil }
        let square = CGRect(x: region.midX - side / 2, y: region.midY - side / 2,
                            width: side, height: side)
        guard let cut = cg.cropping(to: square.integral) else { return nil }
        return UIImage(cgImage: cut, scale: scale, orientation: image.imageOrientation)
    }

    /// 兼容旧名。
    static func cropSquare(_ image: UIImage, mode: BadgeCropMode, normalized: CGRect?) -> UIImage? {
        cropToDeviceSquare(image, mode: mode, normalized: normalized)
    }

    /// 默认自动居中归一化方区（相对原图）。
    static func defaultNormalizedCrop(for image: UIImage) -> CGRect {
        let iw = image.size.width, ih = image.size.height
        let s = min(iw, ih)
        return CGRect(x: (iw - s) / 2 / iw, y: (ih - s) / 2 / ih, width: s / iw, height: s / ih)
    }

    /// 默认自由选区：整图（自选区域初始）。
    static func defaultFreeCrop(for image: UIImage) -> CGRect {
        _ = image
        return CGRect(x: 0, y: 0, width: 1, height: 1)
    }

    private static func rgba466(_ image: UIImage) -> [UInt8]? {
        let px = BadgePacket.px
        let side = CGFloat(px)
        // 按 UIImage 方向绘制，避免竖拍图在设备上横置/空白感
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
