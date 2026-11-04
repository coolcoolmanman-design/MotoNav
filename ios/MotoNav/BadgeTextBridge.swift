import Foundation
import UIKit

/// musicA.18 吧唧文字滚动：文本由 App 渲染成宽位图（黑底彩字，w≤2048 / h≤466，
/// 总面积 ≤466²）走现有 type 7 通道推到 slot 0，再发 #BT,1 切横向滚动；#BT,0 回图片模式。
final class BadgeTextBridge: ObservableObject {
    static let shared = BadgeTextBridge()

    static let textColors: [UIColor] = [
        .white, UIColor(red: 1, green: 0.78, blue: 0.24, alpha: 1),   // 白 / 金
        UIColor(red: 0.3, green: 1, blue: 1, alpha: 1),               // 青
        UIColor(red: 0.3, green: 1, blue: 0.45, alpha: 1),            // 绿
        UIColor(red: 1, green: 0.58, blue: 0.16, alpha: 1),           // 橙
        UIColor(red: 1, green: 0.42, blue: 0.68, alpha: 1),           // 粉
    ]
    static let textColorNames = ["白", "金", "青", "绿", "橙", "粉"]
    static let sizeLabels = ["小", "中", "大", "特大", "超大"]

    private let ud = UserDefaults.standard

    @Published var textMode = UserDefaults.standard.object(forKey: "badge_text_mode") as? Bool ?? false {
        didSet { ud.set(textMode, forKey: "badge_text_mode"); syncTextMode() }
    }
    @Published var badgeText = UserDefaults.standard.string(forKey: "badge_text") ?? "" {
        didSet { ud.set(badgeText, forKey: "badge_text") }
    }
    @Published var textSizeIdx = UserDefaults.standard.object(forKey: "badge_text_size") as? Int ?? 2 {
        didSet { ud.set(textSizeIdx, forKey: "badge_text_size") }
    }
    @Published var textColorIdx = UserDefaults.standard.object(forKey: "badge_text_color") as? Int ?? 0 {
        didSet { ud.set(textColorIdx, forKey: "badge_text_color") }
    }
    @Published private(set) var textStatus = ""

    /// 连接后同步显示模式（#BT）
    func syncTextMode() {
        BleLink.shared.send("#BT,\(textMode ? 1 : 0)")
    }

    /// 固件回执 BT,<mode>
    func textReply(_ mode: Int) {
        textStatus = mode == (textMode ? 1 : 0) ? "已应用" : "保存失败，请重试"
    }

    /// 渲染文字为宽位图并推送（type 7 slot 0），成功后自动切滚动模式。
    func uploadTextToDevice() {
        let t = badgeText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { textStatus = "请先输入文字"; return }
        guard canSend else {
            textStatus = BleLink.shared.isReady ? "导航中不可上传" : "请先连接圆屏"
            return
        }
        textStatus = "正在渲染…"
        let color = Self.textColors[min(textColorIdx, Self.textColors.count - 1)]
        let pt = [34.0, 48.0, 66.0, 88.0, 116.0][min(textSizeIdx, 4)]
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self else { return }
            let font = UIFont.systemFont(ofSize: pt, weight: .bold)
            let attrs: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: color]
            let sz = (t as NSString).size(withAttributes: attrs)
            let stretch: CGFloat = 1.35                       // musicA.19 横向加宽（矢量拉伸，笔画同步变宽）
            var w = min(2048, Int(ceil((sz.width + 120) * stretch)))
            var h = min(466, Int(ceil(sz.height)) + 24)
            if w * h > 466 * 466 { w = max(466, 466 * 466 / h) }   // 面积守卫（固件单槽缓冲 466²）
            let format = UIGraphicsImageRendererFormat()
            format.scale = 1
            format.opaque = true
            let img = UIGraphicsImageRenderer(size: CGSize(width: w, height: h), format: format).image { ctx in
                UIColor.black.setFill()
                UIBezierPath(rect: CGRect(x: 0, y: 0, width: w, height: h)).fill()
                ctx.cgContext.scaleBy(x: stretch, y: 1)       // 矢量级横向加宽，不糊
                (t as NSString).draw(at: CGPoint(x: 60, y: (CGFloat(h) - sz.height) / 2), withAttributes: attrs)
            }
            guard let cg = img.cgImage, let data = Self.rasterFromCG(cg, w: w, h: h) else {
                DispatchQueue.main.async { self.textStatus = "渲染失败" }
                return
            }
            let packet = BadgePacket.headerV2(slot: 0, count: 1, w: w, h: h, data: BadgePacket.zlibForFirmware(data))
            DispatchQueue.main.async {
                guard self.canSend else { self.textStatus = "连接已断开"; return }
                BleLink.shared.sendBulk(type: BadgePacket.bulkType, id: 0, data: packet)
                BadgeBridge.shared.noteTextPushed()
                if !self.textMode { self.textMode = true }          // didSet 自动发 #BT,1
                else { self.syncTextMode() }
                self.textStatus = "已推送 · 圆屏吧唧页横向滚动"
            }
        }
    }

    private var canSend: Bool {
        BleLink.shared.isReady
            && !NaviBridge.shared.navigating
            && !NaviBridge.shared.cruising
            && !WebMotorSession.shared.running
    }

    /// CGImage → RGB565 LE 字节（黑底不透明，无需 alpha 处理）
    private static func rasterFromCG(_ cg: CGImage, w: Int, h: Int) -> [UInt8]? {
        var rgba = [UInt8](repeating: 0, count: w * h * 4)
        guard let ctx = CGContext(data: &rgba, width: w, height: h, bitsPerComponent: 8,
                                  bytesPerRow: w * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.interpolationQuality = .none
        ctx.draw(cg, in: CGRect(x: 0, y: 0, width: w, height: h))
        var out = [UInt8](repeating: 0, count: w * h * 2)
        for i in 0..<(w * h) {
            let r = UInt16(rgba[i * 4]), g = UInt16(rgba[i * 4 + 1]), b = UInt16(rgba[i * 4 + 2])
            let v = (r & 0xF8) << 8 | (g & 0xFC) << 3 | (b >> 3)
            out[i * 2] = UInt8(v & 0xFF)
            out[i * 2 + 1] = UInt8(v >> 8)
        }
        return out
    }
}
