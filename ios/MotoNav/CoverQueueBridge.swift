import Foundation
import UIKit
import MediaPlayer
#if canImport(Compression)
import Compression
#endif

/// FO-225b / musicA.75：音乐主题 7「封面流 Cover Flow」的 3 张真封面预下发。
///
/// 固件侧只有当前封（type 5）与上一首封，AMS 拿不到播放队列的「下一首」封面，
/// 所以这里用「最近播放历史 + 本地曲库其它专辑」凑齐 3 张互不相同的真专辑封面：
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

    /// 每次成功推送当前封面后调用：更新历史并刷新队列全量包。
    func accept(image: UIImage, key: String) {
        guard let raw = Self.rgb565Square(image) else { return }
        if let i = history.firstIndex(where: { $0.key == key }) { history.remove(at: i) }
        history.append(Entry(key: key, rgb565: raw))
        if history.count > maxHistory { history.removeFirst() }
        send()
    }

    /// BLE 重连 / 用户切到主题 7 时补发；没有缓存包但有历史就现编一份。
    func resend() {
        guard BleLink.shared.isReady else { return }
        if lastPacket != nil { sendPacket(lastPacket!) }
        else { send() }
    }

    private func send() {
        guard BleLink.shared.isReady, let cur = history.last else { return }
        let prev = history.count >= 2 ? history[history.count - 2] : nil
        let older = history.count >= 3 ? history[history.count - 3] : nil
        let libs = libraryArts()
        func pick(_ entry: Entry?, _ libIndex: Int) -> [UInt8] {
            if let e = entry { return e.rgb565 }
            if libIndex < libs.count, let r = Self.rgb565Square(libs[libIndex]) { return r }
            // 一首歌都没播过时只剩当前封：镜像 + 冷青偏色，至少不是同一张构图
            return Self.tinted(cur.rgb565)
        }
        let entries: [(Int, [UInt8])] = [
            (0, pick(prev, 0)),
            (1, cur.rgb565),
            (2, pick(older, 1))
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
        lastPacket = d
        sendPacket(d)
    }

    private func sendPacket(_ data: Data) {
        BleLink.shared.sendBulk(type: Self.bulkType, id: seq & 0xFF, data: data)
        seq = (seq + 1) & 0xFF
    }

    /// 本地资料库取两张不同专辑的真封面（离线、只查一次并缓存）。
    private func libraryArts() -> [UIImage] {
        if let c = cachedLibrary { return c }
        var out: [UIImage] = []
        if MPMediaLibrary.authorizationStatus() == .authorized {
            var seen = Set<String>()
            for item in (MPMediaQuery.songs().items ?? []).prefix(240) {
                let album = item.albumTitle ?? "\(item.albumPersistentID)"
                if seen.contains(album) { continue }
                if let im = item.artwork?.image(at: CGSize(width: 160, height: 160)) {
                    seen.insert(album)
                    out.append(im)
                    if out.count >= 2 { break }
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

    /// 只有当前封可用时的占位：水平镜像 + 冷青偏色，保证三张卡不是同一张图。
    private static func tinted(_ raw: [UInt8]) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: raw.count)
        for y in 0..<px {
            for x in 0..<px {
                let si = (y * px + (px - 1 - x)) * 2
                let di = (y * px + x) * 2
                let v = UInt16(raw[si]) | (UInt16(raw[si + 1]) << 8)
                let r = Int((v >> 11) & 31), g = Int((v >> 5) & 63), b = Int(v & 31)
                let nv = UInt16(min(31, r * 10 / 32)) << 11
                    | UInt16(min(63, g * 24 / 32 + 8)) << 5
                    | UInt16(min(31, b * 20 / 32 + 4))
                out[di] = UInt8(nv & 0xFF)
                out[di + 1] = UInt8(nv >> 8)
            }
        }
        return out
    }

    /// RFC1950 zlib（真压缩，约 3~5KB/张）；框架不可用或压缩失败退回 store 流，
    /// 固件 tinfl 两种都能解，CoverPacket.zlibStore 已在 type 5 上验证过。
    private static func zlib(_ raw: [UInt8]) -> Data {
        #if canImport(Compression)
        let cap = raw.count + raw.count / 2 + 1024
        let dst = UnsafeMutablePointer<UInt8>.allocate(capacity: cap)
        defer { dst.deallocate() }
        let n = raw.withUnsafeBytes { (buf: UnsafeRawBufferPointer) -> Int in
            guard let base = buf.baseAddress else { return 0 }
            return compression_encode_buffer(dst, cap,
                                             base.assumingMemoryBound(to: UInt8.self),
                                             raw.count, nil, COMPRESSION_ZLIB)
        }
        if n > 0 { return Data(bytes: dst, count: n) }
        #endif
        return CoverPacket.zlibStore(raw)
    }
}
