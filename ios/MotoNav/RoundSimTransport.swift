#if DEBUG
import Foundation
import Network

/// DEBUG-only TCP bridge to Mac **FirmwareHost** (`mac/FirmwareHost`, port 17875).
/// Framing matches RoundSim: `[u32 LE length][payload]` where payload is one BLE write/notify.
/// Prefer FirmwareHost (real FW sources). `mac/RoundSim` remains an optional framing reference.
final class RoundSimTransport {
    static let defaultHost = "127.0.0.1"
    static let defaultPort: UInt16 = 17875
    static let maxWrite = 500

    private var connection: NWConnection?
    private let decoder = RoundSimFrameDecoder()
    private let queue = DispatchQueue(label: "motonav.roundsim")

    var onStatus: ((String) -> Void)?
    var onReady: (() -> Void)?
    var onDisconnected: (() -> Void)?
    var onPayload: ((Data) -> Void)?

    private(set) var isConnected = false

    func connect(host: String = RoundSimTransport.defaultHost, port: UInt16 = RoundSimTransport.defaultPort) {
        disconnect()
        onStatus?("RoundSim 连接中 \(host):\(port)…")
        let nwHost = NWEndpoint.Host(host)
        guard let nwPort = NWEndpoint.Port(rawValue: port) else {
            onStatus?("RoundSim 端口无效"); return
        }
        let conn = NWConnection(host: nwHost, port: nwPort, using: .tcp)
        connection = conn
        conn.stateUpdateHandler = { [weak self] state in
            DispatchQueue.main.async {
                guard let self else { return }
                switch state {
                case .ready:
                    self.isConnected = true
                    self.onStatus?("RoundSim 已连接 \(host):\(port)")
                    self.onReady?()
                case .failed(let err):
                    self.isConnected = false
                    self.onStatus?("RoundSim 失败：\(err.localizedDescription)")
                    self.onDisconnected?()
                case .cancelled:
                    self.isConnected = false
                    self.onDisconnected?()
                default:
                    break
                }
            }
        }
        conn.start(queue: queue)
        receive(on: conn)
    }

    func disconnect() {
        connection?.cancel()
        connection = nil
        decoder.reset()
        let was = isConnected
        isConnected = false
        if was { DispatchQueue.main.async { self.onDisconnected?() } }
    }

    func send(_ payload: Data) {
        guard let connection, isConnected else { return }
        let frame = RoundSimFrameCodec.encode(payload)
        connection.send(content: frame, completion: .contentProcessed { _ in })
    }

    private func receive(on conn: NWConnection) {
        conn.receive(minimumIncompleteLength: 1, maximumLength: 64 * 1024) { [weak self] data, _, isComplete, error in
            if let data, !data.isEmpty, let self {
                let frames = self.decoder.append(data)
                DispatchQueue.main.async {
                    for frame in frames { self.onPayload?(frame) }
                }
            }
            if isComplete || error != nil {
                DispatchQueue.main.async { self?.disconnect() }
                return
            }
            self?.receive(on: conn)
        }
    }
}

enum RoundSimFrameCodec {
    static let maxPayload = 500
    static func encode(_ payload: Data) -> Data {
        var out = Data(count: 4 + payload.count)
        let n = UInt32(payload.count).littleEndian
        withUnsafeBytes(of: n) { out.replaceSubrange(0..<4, with: $0) }
        out.replaceSubrange(4..., with: payload)
        return out
    }
}

final class RoundSimFrameDecoder {
    private var buffer = Data()
    func append(_ chunk: Data) -> [Data] {
        buffer.append(chunk)
        var frames: [Data] = []
        while buffer.count >= 4 {
            let len = Int(buffer.withUnsafeBytes { $0.load(as: UInt32.self).littleEndian })
            guard len >= 0, len <= RoundSimFrameCodec.maxPayload else {
                buffer.removeAll(keepingCapacity: false); break
            }
            let total = 4 + len
            guard buffer.count >= total else { break }
            frames.append(buffer.subdata(in: 4..<total))
            buffer.removeSubrange(0..<total)
        }
        return frames
    }
    func reset() { buffer.removeAll(keepingCapacity: false) }
}
#endif
