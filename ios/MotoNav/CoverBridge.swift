import Foundation
import Compression
import UIKit
import MediaPlayer

/// 专辑封面推送到圆屏（bulk type 5）。
/// 来源优先级：App 模式直读 nowPlayingItem.artwork → HID 模式本地曲库匹配（标题+歌手）→ iTunes 搜索 API 兜底。
/// 编码：居中裁方 → 128×128 RGB565（小端）→ zlib，头 'C''V''R' 版本 宽 高。
final class CoverBridge {
    static let shared = CoverBridge()
    static let px = 128
    private let cache = NSCache<NSString, UIImage>()
    private var lastKey = ""
    private var seq = 0
    private var fetching = false

    /// 每次歌曲文本变化时调用；title 为空表示没有播放会话，清掉封面。
    func update(title: String, artist: String, item: MPMediaItem?) {
        guard !title.isEmpty else {
            if lastKey != "clear" { lastKey = "clear"; send(header(w: 0, h: 0, data: Data())) }
            return
        }
        let key = title + "|" + artist
        guard key != lastKey, !fetching else { return }
        lastKey = key
        if let img = cachedOrLocal(key: key, item: item, title: title, artist: artist) {
            push(img)
            return
        }
        fetching = true
        fetchArtwork(title: title, artist: artist) { [weak self] img in
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.fetching = false
                if let img = img {
                    self.cache.setObject(img, forKey: key as NSString)
                    self.push(img)
                } else {
                    self.send(self.header(w: 0, h: 0, data: Data()))      // 没找到：清掉上一首的封面
                }
            }
        }
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
        let byTitle = MPMediaQuery()
        byTitle.addFilterPredicate(MPMediaPropertyPredicate(value: title, forProperty: MPMediaItemPropertyTitle))
        if !artist.isEmpty {
            let both = MPMediaQuery()
            both.addFilterPredicate(MPMediaPropertyPredicate(value: title, forProperty: MPMediaItemPropertyTitle))
            both.addFilterPredicate(MPMediaPropertyPredicate(value: artist, forProperty: MPMediaItemPropertyArtist))
            if let art = both.items?.first?.artwork?.image(at: size) { return art }
        }
        return byTitle.items?.first?.artwork?.image(at: size)
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
            let hit = results.first { r in
                let t = (r["trackName"] as? String ?? "").lowercased()
                let a = (r["artistName"] as? String ?? "").lowercased()
                return t.contains(lowTitle) || lowTitle.contains(t) || a.contains(artist.lowercased())
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

    /// 居中裁方 → 缩放到 128×128 → RGB565 → zlib → bulk type 5
    private func push(_ image: UIImage) {
        guard let rgba = Self.rgba128(image) else { send(header(w: 0, h: 0, data: Data())); return }
        var raw = [UInt8](repeating: 0, count: Self.px * Self.px * 2)
        for i in 0..<(Self.px * Self.px) {
            let r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2]
            let v = (UInt16(r) & 0xF8) << 8 | (UInt16(g) & 0xFC) << 3 | (UInt16(b) >> 3)
            raw[i * 2] = UInt8(v & 0xFF)
            raw[i * 2 + 1] = UInt8(v >> 8)
        }
        let z = Self.zlibWrap(raw)
        send(header(w: Self.px, h: Self.px, data: z))
    }

    /// 苹果 COMPRESSION_ZLIB 输出裸 deflate（RFC1951）；固件 parseCover 按 zlib 流（RFC1950）解析，
    /// 需补 0x78 0x9C 头 + adler32 尾（与安卓 java.util.zip.Deflater 产物一致）。
    /// 压缩失败时退回"未压缩存储块"，仍是合法 zlib 流。
    private static func zlibWrap(_ raw: [UInt8]) -> Data {
        var out = Data([0x78, 0x9C])
        if let deflated = try? Data(raw).compressed(using: .zlib) {
            out.append(deflated)
        } else {
            let n = raw.count
            out.append(contentsOf: [0x01,
                UInt8(truncatingIfNeeded: n), UInt8(truncatingIfNeeded: n >> 8),
                UInt8(truncatingIfNeeded: n >> 16), UInt8(truncatingIfNeeded: n >> 24),
                UInt8(truncatingIfNeeded: ~n), UInt8(truncatingIfNeeded: ~(n >> 8)),
                UInt8(truncatingIfNeeded: ~(n >> 16)), UInt8(truncatingIfNeeded: ~(n >> 24))])
            out.append(contentsOf: raw)
        }
        var a: UInt32 = 1, b: UInt32 = 0
        for byte in raw { a = (a + UInt32(byte)) % 65521; b = (b + a) % 65521 }
        let sum = (b << 16) | a
        out.append(UInt8((sum >> 24) & 0xFF))
        out.append(UInt8((sum >> 16) & 0xFF))
        out.append(UInt8((sum >> 8) & 0xFF))
        out.append(UInt8(sum & 0xFF))
        return out
    }

    private func send(_ data: Data) {
        BleLink.shared.sendBulk(type: 5, id: seq & 0xFF, data: data)
        seq = (seq + 1) & 0xFF
    }

    private func header(w: Int, h: Int, data: Data) -> Data {
        var d = Data([UInt8(ascii: "C"), UInt8(ascii: "V"), UInt8(ascii: "R"), 1, UInt8(w), UInt8(h)])
        d.append(data)
        return d
    }

    /// 居中裁方 + 缩放到 128×128，返回 RGBA8 字节
    private static func rgba128(_ image: UIImage) -> [UInt8]? {
        let side = CGFloat(px)
        let scale = image.scale
        let s = min(image.size.width, image.size.height)
        let cropRect = CGRect(x: (image.size.width - s) / 2 * scale, y: (image.size.height - s) / 2 * scale,
                              width: s * scale, height: s * scale)
        guard let crop = image.cgImage?.cropping(to: cropRect) else { return nil }
        var out = [UInt8](repeating: 0, count: px * px * 4)
        guard let ctx = CGContext(data: &out, width: px, height: px, bitsPerComponent: 8, bytesPerRow: px * 4,
                                  space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
        ctx.interpolationQuality = .medium
        ctx.draw(crop, in: CGRect(x: 0, y: 0, width: side, height: side))
        return out
    }
}
