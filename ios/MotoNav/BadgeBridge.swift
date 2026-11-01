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

struct BadgeItem: Identifiable, Equatable {
    let id: UUID
    var source: UIImage
    /// 归一化方选区（相对 source 像素坐标 0…1）；nil = 自动短边居中。
    var normalizedCrop: CGRect?
    var mode: BadgeCropMode
    var preview: UIImage

    static func == (lhs: BadgeItem, rhs: BadgeItem) -> Bool { lhs.id == rhs.id }
}

/// 电子吧唧多图：相册 → 自动居中 / 自选区域裁方 → 466×466 RGB565 → zlib → BLE bulk type 7（ver=2 多槽）。
/// 导航 / 巡航 / 网页模拟导航中禁止发送；开始导航时 `#BM,0` 并清空 type 7 队列。
final class BadgeBridge: ObservableObject {
    static let shared = BadgeBridge()

    @Published private(set) var items: [BadgeItem] = []
    @Published var selectedIndex = 0
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

    var previewImage: UIImage? {
        guard !items.isEmpty else { return nil }
        let i = min(max(0, selectedIndex), items.count - 1)
        return items[i].preview
    }

    var count: Int { items.count }

    /// 选图：追加到相册（最多 `BadgePacket.maxSlots`）；默认自动居中裁方。
    func addPicked(_ image: UIImage) {
        guard items.count < BadgePacket.maxSlots else {
            status = "最多 \(BadgePacket.maxSlots) 张"; return
        }
        guard let square = Self.centerCropSquare(image) else {
            status = "无法裁切图片"; return
        }
        let item = BadgeItem(id: UUID(), source: image, normalizedCrop: nil,
                             mode: .autoCenter, preview: square)
        items.append(item)
        selectedIndex = items.count - 1
        pushed = false
        status = "正在编码…"
        pushAll()
    }

    /// 更新当前选中项的裁切（编辑器「完成」）。
    func applyCrop(mode: BadgeCropMode, normalizedCrop: CGRect?) {
        guard items.indices.contains(selectedIndex) else { return }
        var item = items[selectedIndex]
        item.mode = mode
        item.normalizedCrop = (mode == .manualRegion) ? normalizedCrop : nil
        guard let preview = Self.cropSquare(item.source, mode: mode, normalized: item.normalizedCrop) else {
            status = "无法裁切图片"; return
        }
        item.preview = preview
        items[selectedIndex] = item
        pushed = false
        status = "正在编码…"
        pushAll()
    }

    func selectIndex(_ i: Int) {
        guard items.indices.contains(i) else { return }
        selectedIndex = i
    }

    func removeSelected() {
        guard items.indices.contains(selectedIndex) else { return }
        items.remove(at: selectedIndex)
        if selectedIndex >= items.count { selectedIndex = max(0, items.count - 1) }
        pushed = false
        if items.isEmpty {
            status = ""
            BleLink.shared.cancelBulk(type: BadgePacket.bulkType)
            if canSend {
                BleLink.shared.sendBulk(type: BadgePacket.bulkType, id: seq & 0xFF, data: BadgePacket.clearAll())
                seq = (seq + 1) & 0xFF
            }
        } else {
            pushAll()
        }
    }

    func resendIfPossible() {
        guard canSend, !items.isEmpty else { return }
        pushAll()
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
        items = []
        selectedIndex = 0
        pushed = false
        transferring = false
        status = ""
        BleLink.shared.cancelBulk(type: BadgePacket.bulkType)
    }

    private func pushAll() {
        encodeGeneration += 1
        let gen = encodeGeneration
        let snapshot = items
        guard !snapshot.isEmpty else { return }
        transferring = true
        DispatchQueue.global(qos: .userInitiated).async {
            var packets: [Data] = []
            let count = snapshot.count
            for (slot, item) in snapshot.enumerated() {
                guard let rgba = Self.rgba466(item.preview) else {
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
                guard let packet = BadgePacket.encode(slot: slot, count: count,
                                                      w: BadgePacket.px, h: BadgePacket.px,
                                                      rgb565LE: raw) else {
                    DispatchQueue.main.async {
                        guard self.encodeGeneration == gen else { return }
                        self.transferring = false
                        self.status = "编码失败"
                    }
                    return
                }
                packets.append(packet)
            }
            DispatchQueue.main.async {
                guard self.encodeGeneration == gen else { return }
                self.transferring = false
                guard self.canSend else {
                    self.pushed = false
                    self.status = "已就绪 · 未导航时自动推送"
                    return
                }
                // 先清再按槽推，避免残留旧图
                BleLink.shared.sendBulk(type: BadgePacket.bulkType, id: self.seq & 0xFF,
                                        data: BadgePacket.clearAll())
                self.seq = (self.seq + 1) & 0xFF
                for packet in packets {
                    BleLink.shared.sendBulk(type: BadgePacket.bulkType, id: self.seq & 0xFF, data: packet)
                    self.seq = (self.seq + 1) & 0xFF
                }
                self.pushed = true
                self.status = "已推送到圆屏"
            }
        }
    }

    // MARK: - Crop helpers

    static func centerCropSquare(_ image: UIImage) -> UIImage? {
        cropSquare(image, mode: .autoCenter, normalized: nil)
    }

    /// `normalized`：相对 UIImage.size 的归一化方区（origin+size，边长相等）；manual 模式必填。
    static func cropSquare(_ image: UIImage, mode: BadgeCropMode, normalized: CGRect?) -> UIImage? {
        let scale = image.scale
        guard let cg = image.cgImage else { return nil }
        let iw = image.size.width, ih = image.size.height
        guard iw > 0, ih > 0 else { return nil }

        let crop: CGRect
        if mode == .manualRegion, let n = normalized, n.width > 0, n.height > 0 {
            let side = min(n.width, n.height)
            var x = n.origin.x, y = n.origin.y
            x = max(0, min(x, 1 - side))
            y = max(0, min(y, 1 - side))
            crop = CGRect(x: x * iw * scale, y: y * ih * scale,
                          width: side * iw * scale, height: side * ih * scale)
        } else {
            let s = min(iw, ih)
            crop = CGRect(x: (iw - s) / 2 * scale, y: (ih - s) / 2 * scale,
                          width: s * scale, height: s * scale)
        }
        guard let cut = cg.cropping(to: crop.integral) else { return nil }
        return UIImage(cgImage: cut, scale: scale, orientation: image.imageOrientation)
    }

    /// 默认自动居中归一化方区（相对原图）。
    static func defaultNormalizedCrop(for image: UIImage) -> CGRect {
        let iw = image.size.width, ih = image.size.height
        let s = min(iw, ih)
        return CGRect(x: (iw - s) / 2 / iw, y: (ih - s) / 2 / ih, width: s / iw, height: s / ih)
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
