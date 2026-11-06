import Foundation
import AppKit
import Combine
import Compression

enum SimPage: String, CaseIterable {
    case idle, music, nav, arrival
}

@MainActor
final class SimState: ObservableObject {
    static let lcdSize: CGFloat = 466

    @Published var page: SimPage = .idle
    @Published var connectedClients = 0
    @Published var lastCommand = ""
    @Published var logLines: [String] = []
    @Published var injectText = "#v,42"
    @Published var showBezel = true
    @Published var simulateDeviceRawTouch = false

    // Idle / clock
    @Published var clockTick = Date()

    // Music
    @Published var musicHave = false
    @Published var musicPlaying = false
    @Published var coverSpin = true
    @Published var coverAngle: Double = 0
    @Published var coverImage: NSImage?
    @Published var titleSlot: String = ""
    @Published var artistSlot: String = ""
    @Published var tonearmT: Double = 0 // 0 raised, 1 on vinyl

    // Nav
    @Published var inNav = false
    @Published var navDir = ""
    @Published var navDist = 0
    @Published var speedKmh = 0
    @Published var remainM = 0
    @Published var etaHHMM = ""
    @Published var mapOn = false
    @Published var nightOn = false
    @Published var gpsWeak = false
    @Published var rerouting = false
    @Published var brightness = 180
    @Published var mapBulkKB = 0
    @Published var mapStatus = "no map bulk"

    // Arrival (musicA.13 stay-until-OK)
    @Published var arrivalOn = false
    @Published var arrivalWaypoint = false
    @Published var tripM = -1
    @Published var tripMin = -1
    @Published var tripMaxKmh = -1
    @Published var arrivalStartedAt = Date()

    let bulk = BulkReceiver()
    let bitmaps = TextBitmapReceiver()
    var sendNotify: ((String) -> Void)?
    var sendBinary: ((Data) -> Void)?

    private var clockTimer: Timer?
    private var spinTimer: Timer?

    init() {
        bulk.onAck = { [weak self] ack in
            Task { @MainActor in self?.emit(ack) }
        }
        bulk.onComplete = { [weak self] type, id, data in
            Task { @MainActor in self?.didCompleteBulk(type: type, id: id, data: data) }
        }
        bitmaps.onUpdate = { [weak self] slot in
            Task { @MainActor in self?.didUpdateBitmap(slot) }
        }
        clockTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.clockTick = Date() }
        }
        spinTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 24.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.tickSpin() }
        }
    }

    func log(_ line: String) {
        let stamp = ISO8601DateFormatter().string(from: Date()).suffix(8)
        logLines.append("\(stamp) \(line)")
        if logLines.count > 200 { logLines.removeFirst(logLines.count - 200) }
    }

    func emit(_ text: String) {
        log("→ \(text)")
        sendNotify?(text)
    }

    func handleInbound(_ payload: Data) {
        guard let first = payload.first else { return }
        if first == UInt8(ascii: "M") {
            bulk.handle(payload)
            return
        }
        if first == UInt8(ascii: "B") {
            bitmaps.handle(payload)
            return
        }
        if let s = String(data: payload, encoding: .utf8) {
            CommandParser.apply(s, to: self)
        }
    }

    func injectManual() {
        let lines = injectText.split(whereSeparator: { $0.isNewline })
        for line in lines {
            let s = String(line).trimmingCharacters(in: .whitespaces)
            guard !s.isEmpty else { continue }
            if let d = s.data(using: .utf8) { handleInbound(d) }
        }
    }

    func onClientConnected() {
        connectedClients += 1
        // Mirror FW capability probes / verified handshake pieces BleLink understands.
        emit("U,1")
        emit("CAP,BLD,1")
        emit("CAP,HID,1")
        emit("CAP,LYR,1")
        log("client connected (#\(connectedClients))")
    }

    func onClientDisconnected() {
        connectedClients = max(0, connectedClients - 1)
        log("client disconnected")
    }

    func resetNav() {
        inNav = false
        navDir = ""
        navDist = 0
        speedKmh = 0
        remainM = 0
        etaHHMM = ""
        gpsWeak = false
        rerouting = false
    }

    func startArrival(waypoint: Bool) {
        arrivalOn = true
        arrivalWaypoint = waypoint
        arrivalStartedAt = Date()
        page = .arrival
    }

    func stopArrival() {
        arrivalOn = false
    }

    /// musicA.13: destination OK → progReset + resetNavState equivalent.
    func hitArrivalOK() {
        guard arrivalOn else { return }
        let dest = !arrivalWaypoint
        stopArrival()
        if dest { resetNav() }
        page = .idle
        log("OK hit → clear arrival\(dest ? " + reset nav" : "")")
    }

    func handleCanvasTap(_ point: CGPoint) {
        let canvas = TouchMapper.canvasPoint(from: point, simulateDeviceRaw: simulateDeviceRawTouch)
        log(String(format: "touch canvas (%.0f,%.0f)%@", canvas.x, canvas.y,
                   simulateDeviceRawTouch ? " [raw-sim]" : ""))
        if arrivalOn {
            if arrivalWaypoint || TouchMapper.fxOkHit(canvas: canvas) {
                hitArrivalOK()
            }
            return
        }
        // Soft page switch by tap zones for manual debug (not FW swipe).
        if page == .idle, canvas.x > 300 { page = .music }
        else if page == .music, canvas.x < 160 { page = .idle }
    }

    private func tickSpin() {
        let target: Double = (musicPlaying && coverSpin && coverImage != nil) ? 1 : 0
        tonearmT += (target - tonearmT) * 0.18
        if abs(target - tonearmT) < 0.01 { tonearmT = target }
        if musicPlaying && coverSpin && coverImage != nil {
            coverAngle += (2 * Double.pi) / 36.0 / 24.0 // ~36s/rev like FW
            if coverAngle > 2 * Double.pi { coverAngle -= 2 * Double.pi }
        }
    }

    private func didCompleteBulk(type: Int, id: Int, data: Data) {
        switch type {
        case 5:
            decodeCover(data)
            page = .music
            log("bulk cover id=\(id) \(data.count)B")
        case 1, 2, 3, 4:
            mapBulkKB += data.count / 1024
            mapStatus = "ACK type \(type) id \(id) · \(data.count) B (stub render)"
            if mapOn || inNav { page = .nav }
            log("bulk map-ish type=\(type) id=\(id) \(data.count)B → ACK")
        default:
            break
        }
    }

    private func didUpdateBitmap(_ slot: Int) {
        guard let img = bitmaps.images[slot], img.w > 0 else { return }
        // Slots 5/6 = title/artist text bitmaps — show dimensions only in MVP.
        if slot == 5 { titleSlot = "title \(img.w)×\(img.h)" }
        if slot == 6 { artistSlot = "artist \(img.w)×\(img.h)" }
        if slot == 5 || slot == 6 { page = .music }
    }

    /// Cover payload: `'CVR' + ver + w + h + zlib(RGB565)`.
    private func decodeCover(_ data: Data) {
        guard data.count > 6,
              data[0] == UInt8(ascii: "C"),
              data[1] == UInt8(ascii: "V"),
              data[2] == UInt8(ascii: "R") else {
            coverImage = placeholderCover(tint: .gray)
            return
        }
        let w = Int(data[4]), h = Int(data[5])
        let z = data.subdata(in: 6..<data.count)
        guard let raw = inflateZlib(z), w > 0, h > 0, raw.count >= w * h * 2 else {
            coverImage = placeholderCover(tint: .cyan)
            return
        }
        coverImage = rgb565Image(raw: raw, w: w, h: h) ?? placeholderCover(tint: .orange)
    }

    private func placeholderCover(tint: NSColor) -> NSImage {
        let img = NSImage(size: NSSize(width: 128, height: 128))
        img.lockFocus()
        tint.withAlphaComponent(0.35).setFill()
        NSBezierPath(ovalIn: NSRect(x: 0, y: 0, width: 128, height: 128)).fill()
        img.unlockFocus()
        return img
    }

    private func inflateZlib(_ data: Data) -> Data? {
        // CoverBridge ships RFC1950 zlib; Apple COMPRESSION_ZLIB expects that wrapper.
        guard data.count > 6 else { return nil }
        let dstCapacity = 128 * 128 * 2 * 4
        var dst = Data(count: dstCapacity)
        let decoded: Int = data.withUnsafeBytes { srcBuf in
            dst.withUnsafeMutableBytes { dstBuf in
                guard let src = srcBuf.bindMemory(to: UInt8.self).baseAddress,
                      let out = dstBuf.bindMemory(to: UInt8.self).baseAddress else { return 0 }
                return compression_decode_buffer(out, dstCapacity, src, data.count, nil, COMPRESSION_ZLIB)
            }
        }
        guard decoded > 0 else { return nil }
        return dst.prefix(decoded)
    }

    private func rgb565Image(raw: Data, w: Int, h: Int) -> NSImage? {
        var rgba = [UInt8](repeating: 0, count: w * h * 4)
        for i in 0..<(w * h) {
            let lo = Int(raw[i * 2]), hi = Int(raw[i * 2 + 1])
            let v = lo | (hi << 8)
            let r = UInt8(((v >> 11) & 0x1F) * 255 / 31)
            let g = UInt8(((v >> 5) & 0x3F) * 255 / 63)
            let b = UInt8((v & 0x1F) * 255 / 31)
            rgba[i * 4] = r; rgba[i * 4 + 1] = g; rgba[i * 4 + 2] = b; rgba[i * 4 + 3] = 255
        }
        let rep = NSBitmapImageRep(
            bitmapDataPlanes: nil, pixelsWide: w, pixelsHigh: h,
            bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
            colorSpaceName: .deviceRGB, bytesPerRow: w * 4, bitsPerPixel: 32)
        guard let rep else { return nil }
        rgba.withUnsafeBytes { src in
            if let dest = rep.bitmapData {
                memcpy(dest, src.baseAddress!, rgba.count)
            }
        }
        let img = NSImage(size: NSSize(width: w, height: h))
        img.addRepresentation(rep)
        return img
    }
}
