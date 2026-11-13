import Foundation
import UIKit
import MediaPlayer

/// FO-225b / musicA.75：音乐主题 7「封面流 Cover Flow」的 3 张真封面预下发。
/// FO-225c：修「实机所有封面都一样」——歌曲未变 / BLE 重连时队列桥也必须能发包，
/// 且三张卡必须像素明显不同（历史真封 > 曲库真封 > 两种冷暖色调 + 不同裁切变体）。
///
/// 固件侧只有当前封（type 5），AMS 拿不到播放队列的「下一首」封面：
///   slot 0 = 上一首 / 历史        slot 1 = 当前封        slot 2 = 下一首候选 / 其它专辑
/// 手机真的切歌后 AMS 推来新封面，本桥会重发队列覆盖，固件 `cfShift` 归零。
///
/// bulk type 12：'C''F''Q' ver=1 count + count×{ slot(u8), zlen(u16 LE), zlib(64² RGB565 LE) }
/// 只在切歌 / 首次进入音乐页 / 重连时发，不在滑动时实时取（BLE ≈5KB/s 来不及）。
final class CoverQueueBridge {
    static let shared = CoverQueueBridge()
    static let px = 64
    static let bulkType = 12

    private struct Entry {
        let key: String
        let rgb565: [UInt8]
    }
    private var history: [Entry] = []
    private var cachedLibrary: [UIImage]?
    private var lastPacket: Data?
    private var seq = 0
    private let maxHistory = 4

    /// 每次拿到当前封面都调用（含去重早退路径）：内部按 key 去重，不反复发包。
    func accept(image: UIImage, key: String) {
        guard let raw = Self.rgb565Square(image) else { return }
        if history.last?.key == key {
            history[history.count - 1] = Entry(key: key, rgb565: raw)
            if lastPacket != nil { return }           // 同一首且已发过：只更新像素
        } else {
            if let i = history.firstIndex(where: { $0.key == key }) { history.remove(at: i) }
            history.append(Entry(key: key, rgb565: raw))
            if history.count > maxHistory { history.removeFirst() }
        }
        send()
    }

    /// BLE 重连 / 用户切到主题 7 时补发；没有缓存包但有历史就现编一份。
    func resend() {
        guard BleLink.shared.isReady else { return }
        if lastPacket != nil { sendPacket(lastPacket!) }
        else { send() }
    }

    private func send() {
        guard let cur = history.last else { return }
        let prev = history.count >= 2 ? history[history.count - 2] : nil
        let older = history.count >= 3 ? history[history.count - 3] : nil
        let libs = libraryArts()
        func pick(_ entry: Entry?, _ libIndex: Int, _ seed: Int) -> [UInt8] {
            if let e = entry { return e.rgb565 }
            if libIndex < libs.count, let r = Self.rgb565Square(libs[libIndex]) { return r }
            // 一首歌都没播过 / 本地曲库没封面：两个明显不同的变体，别再让三张卡同图
            return Self.variant(cur.rgb565, seed: seed)
        }
        let entries: [(Int, [UInt8])] = [
            (0, pick(prev, 0, 0)),
            (1, cur.rgb565),
            (2, pick(older, 1, 2))
        ]
        var d = Data([UInt8(ascii: "C"), UInt8(ascii: "F"), UInt8(ascii: "Q"), 1,
                      UInt8(entries.count)])
        for (slot, raw) in entries {
            let z = Self.zlib(raw)
            let n = UInt16(z.count)
            d.append(UInt8(slot))
            d.append(UInt8(n & 0xFF))
            d.append(UInt8(n >> 8))
            d.append(z)
        }
        lastPacket = d                       // FO-225c：先缓存，连接没就绪也能靠 resend 补发
        guard BleLink.shared.isReady else { return }
        sendPacket(d)
    }

    private func sendPacket(_ data: Data) {
        BleLink.shared.sendBulk(type: Self.bulkType, id: seq & 0xFF, data: data)
        seq = (seq + 1) & 0xFF
    }

    /// 本地资料库取两张不同专辑的真封面（离线、只查一次并缓存）。
    /// FO-225c：优先按专辑集合查（HID/AMS 模式下本地单曲可能一首都没有），再用单曲兜底。
    private func libraryArts() -> [UIImage] {
        if let c = cachedLibrary { return c }
        var out: [UIImage] = []
        if MPMediaLibrary.authorizationStatus() == .authorized {
            let size = CGSize(width: 160, height: 160)
            var seen = Set<String>()
            for coll in (MPMediaQuery.albums().collections ?? []).prefix(80) {
                let album = coll.representativeItem?.albumTitle ?? "\(coll.persistentID)"
                if seen.contains(album) { continue }
                if let im = coll.representativeItem?.artwork?.image(at: size) {
                    seen.insert(album)
                    out.append(im)
                    if out.count >= 2 { break }
                }
            }
            if out.count < 2 {
                for item in (MPMediaQuery.songs().items ?? []).prefix(240) {
                    let album = item.albumTitle ?? "\(item.albumPersistentID)"
                    if seen.contains(album) { continue }
                    if let im = item.artwork?.image(at: size) {
                        seen.insert(album)
                        out.append(im)
                        if out.count >= 2 { break }
                    }
                }
            }
        }
        cachedLibrary = out
        return out
    }

    /// 居中裁方 → 64² → RGB565 小端（与 CoverBridge 的 type 5 编码一致）。
    private static func rgb565Square(_ image: UIImage) -> [UInt8]? {
        let side = CGFloat(px)
        let scale = image.scale
        let s = min(image.size.width, image.size.height)
        guard s > 0 else { return nil }
        let cropRect = CGRect(x: (image.size.width - s) / 2 * scale,
                              y: (image.size.height - s) / 2 * scale,
                              width: s * scale, height: s * scale)
        guard let crop = image.cgImage?.cropping(to: cropRect) else { return nil }
        var rgba = [UInt8](repeating: 0, count: px * px * 4)
        guard let ctx = CGContext(data: &rgba, width: px, height: px, bitsPerComponent: 8,
                                  bytesPerRow: px * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.interpolationQuality = .high
        ctx.draw(crop, in: CGRect(x: 0, y: 0, width: side, height: side))
        var out = [UInt8](repeating: 0, count: px * px * 2)
        for i in 0..<(px * px) {
            let r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2]
            let v = (UInt16(r) & 0xF8) << 8 | (UInt16(g) & 0xFC) << 3 | (UInt16(b) >> 3)
            out[i * 2] = UInt8(v & 0xFF)
            out[i * 2 + 1] = UInt8(v >> 8)
        }
        return out
    }

    /// FO-225c：两个明显不同的变体（镜像方向 + 冷青 / 暖紫 + 裁切），
    /// 替代原来两次调用同一个 tinted() 导致左右卡看起来完全相同。
    private static func variant(_ raw: [UInt8], seed: Int) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: raw.count)
        for y in 0..<px {
            for x in 0..<px {
                let sx = seed == 0 ? (px - 1 - x) : x
                let sy = seed == 0 ? y : (px - 1 - y)
                let si = (sy * px + sx) * 2, di = (y * px + x) * 2
                let v = UInt16(raw[si]) | (UInt16(raw[si + 1]) << 8)
                var r = Int((v >> 11) & 31), g = Int((v >> 5) & 63), b = Int(v & 31)
                if seed == 0 {
                    r = r * 10 / 32; g = min(63, g * 24 / 32 + 8); b = min(31, b * 20 / 32 + 4)
                } else {
                    r = min(31, r * 26 / 32 + 5); g = g * 16 / 32; b = min(31, b * 22 / 32 + 6)
                }
                let nv: UInt16 = (UInt16(r) << 11) | (UInt16(g) << 5) | UInt16(b)
                out[di] = UInt8(nv & 0xFF)
                out[di + 1] = UInt8(nv >> 8)
            }
        }
        return out
    }

    /// FO-225d：固件 tinfl 只认 RFC1950 zlib。Apple `Compression` 的 COMPRESSION_ZLIB
    /// 实际输出 raw DEFLATE（实机日志 `[CFQ] slot=N FAIL st=-1 in=2 out=0` = 读完 2 字节
    /// zlib 头就判失败），所以统一改用 CoverPacket.zlibStore() 的标准 zlib store 流，
    /// 与 type 5 当前封面同一套编码。每张 8203 B，整包约 24.6 KB（仍在 CFQ_MAX 内），
    /// 只在切歌 / 首次进音乐页 / 重连发一次。
    private static func zlib(_ raw: [UInt8]) -> Data {
        return CoverPacket.zlibStore(raw)
    }
}
