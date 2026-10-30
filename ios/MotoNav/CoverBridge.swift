import Foundation
import UIKit
import MediaPlayer

/// 专辑封面推送到圆屏（bulk type 5）。
/// 来源优先级：App 模式直读 nowPlayingItem.artwork → HID 模式本地曲库匹配 → iTunes 搜索兜底。
/// 编码：居中裁方 → 128×128 RGB565（小端）→ zlib store（见 `CoverPacket`）→ 'CVR' 头。
final class CoverBridge {
    static let shared = CoverBridge()
    private let cache = NSCache<NSString, UIImage>()
    private var lastKey = ""
    private var lastPushedArt = false
    private var lastPacket: Data?
    private var seq = 0
    private var fetchingKey: String?
    private var fetchGeneration = 0

    /// 每次歌曲文本变化时调用；title 为空表示没有播放会话，清掉封面。
    /// `title` / `artist` 必须是干净曲名（不要带 AMS 截断的 "…" 显示后缀）。
    func update(title: String, artist: String, item: MPMediaItem?) {
        guard !title.isEmpty else {
            clearOnScreen()
            return
        }
        let key = title + "|" + artist
        let local = cachedOrLocal(key: key, item: item, title: title, artist: artist)
        // 去重键带上"本地封面是否可用"：封面比文本晚到时仍能补发（对齐 Android CoverBridge）。
        let dedupe = key + "|" + (local != nil ? "1" : "0")
        if let img = local {
            if dedupe == lastKey, lastPushedArt { return }
            lastKey = dedupe
            fetchingKey = nil
            fetchGeneration += 1
            push(img, trackKey: key)
            return
        }
        // No local art: one iTunes attempt per track; AMS ticks must not re-hit the network.
        if dedupe == lastKey || fetchingKey == key { return }
        lastKey = dedupe
        startItunesFetch(title: title, artist: artist, key: key)
    }

    /// BLE 重连后重发上一张已编码封面（圆屏可能丢过 type 5）。
    func resendLast() {
        guard let packet = lastPacket, BleLink.shared.isReady else { return }
        send(packet)
    }

    func reset() {
        lastKey = ""; lastPushedArt = false; lastPacket = nil
        fetchingKey = nil; fetchGeneration += 1
    }

    private func clearOnScreen() {
        if lastKey == "clear", !lastPushedArt { return }
        lastKey = "clear"; lastPushedArt = false; lastPacket = nil
        fetchingKey = nil; fetchGeneration += 1
        send(CoverPacket.clear())
    }

    private func cachedOrLocal(key: String, item: MPMediaItem?, title: String, artist: String) -> UIImage? {
        if let img = cache.object(forKey: key as NSString) { return img }
        if let art = item?.artwork?.image(at: CGSize(width: 256, height: 256)) { return art }
        return libraryArtwork(title: title, artist: artist)
    }

    /// 本地曲库匹配（离线；对网易云/QQ 音乐的歌只在同时存在于资料库时有封面）
    private func libraryArtwork(title: String, artist: String) -> UIImage? {
        guard MPMediaLibrary.authorizationStatus() == .authorized, !title.isEmpty else { return nil }
        let size = CGSize(width: 256, height: 256)
        if !artist.isEmpty {
            let both = MPMediaQuery()
            both.addFilterPredicate(MPMediaPropertyPredicate(value: title, forProperty: MPMediaItemPropertyTitle))
            both.addFilterPredicate(MPMediaPropertyPredicate(value: artist, forProperty: MPMediaItemPropertyArtist))
            if let art = both.items?.first?.artwork?.image(at: size) { return art }
        }
        let byTitle = MPMediaQuery()
        byTitle.addFilterPredicate(MPMediaPropertyPredicate(value: title, forProperty: MPMediaItemPropertyTitle))
        return byTitle.items?.first?.artwork?.image(at: size)
    }

    private func startItunesFetch(title: String, artist: String, key: String) {
        fetchingKey = key
        fetchGeneration += 1
        let gen = fetchGeneration
        fetchArtwork(title: title, artist: artist) { [weak self] img in
            DispatchQueue.main.async {
                guard let self = self, self.fetchGeneration == gen, self.fetchingKey == key else { return }
                self.fetchingKey = nil
                if let img = img {
                    self.cache.setObject(img, forKey: key as NSString)
                    self.push(img, trackKey: key)
                } else {
                    // 未找到：清屏但不永久锁死——下次带本地图的 update 仍可补发。
                    self.lastPushedArt = false
                    self.lastPacket = nil
                    self.send(CoverPacket.clear())
                }
            }
        }
    }

    /// iTunes 搜索兜底（联网）。artworkUrl100 把 100x100bb 换成 600x600bb 拿高清图。
    private func fetchArtwork(title: String, artist: String, done: @escaping (UIImage?) -> Void) {
        var comps = URLComponents(string: "https://itunes.apple.com/search")!
        var term = title
        if !artist.isEmpty { term += " " + artist }
        comps.queryItems = [URLQueryItem(name: "term", value: term),
                            URLQueryItem(name: "entity", value: "song"),
                            URLQueryItem(name: "limit", value: "5")]
        guard let url = comps.url else { done(nil); return }
        download(url) { data in
            guard let data = data,
                  let obj = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any],
                  let results = obj["results"] as? [[String: Any]] else { done(nil); return }
            let lowTitle = title.lowercased()
            let lowArtist = artist.lowercased()
            let hit = results.first { r in
                let t = (r["trackName"] as? String ?? "").lowercased()
                let a = (r["artistName"] as? String ?? "").lowercased()
                return t.contains(lowTitle) || lowTitle.contains(t) || (!lowArtist.isEmpty && a.contains(lowArtist))
            } ?? results.first
            guard let urlStr = hit?["artworkUrl100"] as? String,
                  let big = URL(string: urlStr.replacingOccurrences(of: "100x100bb", with: "600x600bb")) else { done(nil); return }
            self.download(big) { imgData in done(imgData.flatMap(UIImage.init(data:))) }
        }
    }

    private func download(_ url: URL, done: @escaping (Data?) -> Void) {
        var req = URLRequest(url: url)
        req.timeoutInterval = 10
        URLSession.shared.dataTask(with: req) { data, resp, _ in
            let ok = (resp as? HTTPURLResponse)?.statusCode == 200
            done(ok ? data : nil)
        }.resume()
    }

    /// 居中裁方 → 128×128 → RGB565 LE → CoverPacket（store zlib）→ bulk type 5
    private func push(_ image: UIImage, trackKey: String) {
        guard let rgba = Self.rgba128(image) else {
            lastPushedArt = false; send(CoverPacket.clear()); return
        }
        var raw = [UInt8](repeating: 0, count: CoverPacket.px * CoverPacket.px * 2)
        for i in 0..<(CoverPacket.px * CoverPacket.px) {
            let r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2]
            let v = (UInt16(r) & 0xF8) << 8 | (UInt16(g) & 0xFC) << 3 | (UInt16(b) >> 3)
            raw[i * 2] = UInt8(v & 0xFF)
            raw[i * 2 + 1] = UInt8(v >> 8)
        }
        guard let packet = CoverPacket.encode(w: CoverPacket.px, h: CoverPacket.px, rgb565LE: raw) else {
            lastPushedArt = false; send(CoverPacket.clear()); return
        }
        lastKey = trackKey + "|1"
        lastPushedArt = true
        lastPacket = packet
        send(packet)
    }

    private func send(_ data: Data) {
        BleLink.shared.sendBulk(type: CoverPacket.bulkType, id: seq & 0xFF, data: data)
        seq = (seq + 1) & 0xFF
    }

    /// 居中裁方 + 缩放到 128×128，返回 RGBA8 字节
    private static func rgba128(_ image: UIImage) -> [UInt8]? {
        let side = CGFloat(CoverPacket.px)
        let scale = image.scale
        let s = min(image.size.width, image.size.height)
        guard s > 0 else { return nil }
        let cropRect = CGRect(x: (image.size.width - s) / 2 * scale, y: (image.size.height - s) / 2 * scale,
                              width: s * scale, height: s * scale)
        guard let crop = image.cgImage?.cropping(to: cropRect) else { return nil }
        var out = [UInt8](repeating: 0, count: CoverPacket.px * CoverPacket.px * 4)
        guard let ctx = CGContext(data: &out, width: CoverPacket.px, height: CoverPacket.px, bitsPerComponent: 8,
                                  bytesPerRow: CoverPacket.px * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.interpolationQuality = .medium
        ctx.draw(crop, in: CGRect(x: 0, y: 0, width: side, height: side))
        return out
    }
}
