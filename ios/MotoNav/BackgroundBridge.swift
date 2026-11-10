import Foundation
import UIKit
import Combine

/// 待机壁纸（musicA.17 自定义开机背景，musicA.21 改名）：相册选图 → 居中裁方 → 466×466 RGB565 → zlib → BLE bulk type 8。
/// musicA.46：圆屏 ffat 持久化；认证后若收到 BGR,cached,1 则跳过强制重推，仅用户变更/清除时再推。
/// App 仍把图存本地（Documents/bg_custom.png）作编辑底稿。
final class BackgroundBridge: ObservableObject {
    static let shared = BackgroundBridge()

    @Published private(set) var previewImage: UIImage?
    @Published private(set) var status = ""
    @Published private(set) var transferring = false
    @Published var pushed = false
    // musicA.18：与电子吧唧一致的裁剪链路（原图 + 裁剪模式 + 归一化选区）
    @Published private(set) var sourceImage: UIImage?
    @Published private(set) var cropMode: BadgeCropMode = .autoCenter
    @Published private(set) var normalizedCrop: CGRect?
    /// musicA.46：设备已从 Flash 装回 / 本会话刚推成功 → CAP,BGR 不再整图重推
    private var skipAutoResend = false
    /// musicA.61 / FO-203：当前 type8 壁纸是否为 Infograph 烘焙底图（切出时须清，勿误清用户相册壁纸）
    private(set) var infographBaseActive = false

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

    /// 仅选图预览，不推送（musicA.18：保留原图，可用裁剪编辑器自选区域）。
    func selectImage(_ image: UIImage) {
        guard let square = BadgeBridge.centerCropSquare(image) else {
            status = "无法裁切图片"; return
        }
        sourceImage = image
        cropMode = .autoCenter
        normalizedCrop = nil
        previewImage = square
        pushed = false
        skipAutoResend = false
        infographBaseActive = false
        status = "已选择 · 点「推送到圆屏」设为待机壁纸"
        saveStoredImage(square)
    }

    /// musicA.18：裁剪编辑器「完成」回调（复用吧唧裁剪管线）。
    func applyCrop(mode: BadgeCropMode, normalizedCrop: CGRect?) {
        guard let source = sourceImage else { return }
        cropMode = mode
        self.normalizedCrop = (mode == .manualRegion) ? normalizedCrop : nil
        guard let preview = BadgeBridge.cropToDeviceSquare(source, mode: mode, normalized: self.normalizedCrop) else {
            status = "无法裁切图片"; return
        }
        previewImage = preview
        pushed = false
        skipAutoResend = false
        infographBaseActive = false
        status = "已裁切 · 点「推送到圆屏」设为待机壁纸"
        saveStoredImage(preview)
    }

    func clearImage() {
        encodeGeneration += 1
        sourceImage = nil
        cropMode = .autoCenter
        normalizedCrop = nil
        previewImage = nil
        pushed = false
        skipAutoResend = false
        infographBaseActive = false
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

    /// FO-203：切出 Infograph 时清掉烘焙底图残影；用户相册壁纸不动。
    func clearInfographBaseIfActive() {
        guard infographBaseActive else { return }
        clearImage()
    }

    /// 固件 BGR,cached,1：Flash 已有壁纸，跳过 CAP 触发的强制重推。
    func noteDeviceCached() {
        skipAutoResend = true
        if previewImage != nil { pushed = true }
        status = "圆屏已有壁纸 · 无需重推"
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

    /// musicA.58：加载 Infograph 烘焙底图（Assets `InfographDialBase`）并可选推送到圆屏。
    /// C Infograph 依赖 type8 壁纸承载刻度/子盘铬/灰轨；清除壁纸会露黑底。
    func applyInfographBase(push: Bool) {
        guard let img = UIImage(named: "InfographDialBase") else {
            status = "缺少 Infograph 底图资源"
            return
        }
        sourceImage = img
        cropMode = .autoCenter
        normalizedCrop = nil
        previewImage = img
        pushed = false
        skipAutoResend = false
        infographBaseActive = true
        saveStoredImage(img)
        status = push
            ? (canSend ? "Infograph 底图就绪 · 正在下发…" : "Infograph 底图已载入 · 连接后自动下发")
            : "Infograph 底图已载入 · 点「推送到圆屏」下发"
        if push { pushToDevice() }
    }

    /// 连接后自动重推（CAP,BGR,1 触发）。musicA.46：设备已 cached 则跳过。
    func resendIfPossible() {
        guard canSend, let preview = previewImage else { return }
        if skipAutoResend { return }
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
                self.skipAutoResend = true
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
