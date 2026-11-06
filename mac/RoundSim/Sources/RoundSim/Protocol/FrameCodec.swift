import Foundation

/// TCP framing: one BLE ATT write/notify ↔ one framed message.
/// Layout: `[u32 LE length][payload…]`. Payload max matches BleLink `MAX_WRITE` (500).
enum FrameCodec {
    static let maxPayload = 500

    static func encode(_ payload: Data) -> Data {
        precondition(payload.count <= maxPayload)
        var out = Data(count: 4 + payload.count)
        let n = UInt32(payload.count).littleEndian
        withUnsafeBytes(of: n) { out.replaceSubrange(0..<4, with: $0) }
        out.replaceSubrange(4..., with: payload)
        return out
    }

    static func encodeText(_ text: String) -> Data {
        encode(Data(text.utf8))
    }
}

/// Incremental stream decoder for length-prefixed frames.
final class FrameDecoder {
    private var buffer = Data()

    func append(_ chunk: Data) -> [Data] {
        buffer.append(chunk)
        var frames: [Data] = []
        while buffer.count >= 4 {
            let len = buffer.withUnsafeBytes { $0.load(as: UInt32.self).littleEndian }
            guard len <= FrameCodec.maxPayload else {
                buffer.removeAll(keepingCapacity: false)
                break
            }
            let total = 4 + Int(len)
            guard buffer.count >= total else { break }
            frames.append(buffer.subdata(in: 4..<total))
            buffer.removeSubrange(0..<total)
        }
        return frames
    }

    func reset() { buffer.removeAll(keepingCapacity: false) }
}
