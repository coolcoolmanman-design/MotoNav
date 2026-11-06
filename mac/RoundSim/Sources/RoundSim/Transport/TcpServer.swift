import Foundation
import Network

/// Local TCP server that speaks RoundSim framing on `127.0.0.1:17875` by default.
@MainActor
final class TcpServer: ObservableObject {
    static let defaultPort: UInt16 = 17875

    @Published var status = "idle"
    @Published var port: UInt16 = TcpServer.defaultPort

    private var listener: NWListener?
    private var connection: NWConnection?
    private let decoder = FrameDecoder()
    private let queue = DispatchQueue(label: "roundsim.tcp")

    weak var state: SimState?

    func start(port: UInt16 = TcpServer.defaultPort) {
        stop()
        self.port = port
        do {
            let params = NWParameters.tcp
            params.allowLocalEndpointReuse = true
            listener = try NWListener(using: params, on: NWEndpoint.Port(rawValue: port)!)
        } catch {
            status = "listen failed: \(error.localizedDescription)"
            return
        }
        listener?.newConnectionHandler = { [weak self] conn in
            Task { @MainActor in self?.accept(conn) }
        }
        listener?.stateUpdateHandler = { [weak self] st in
            Task { @MainActor in
                switch st {
                case .ready: self?.status = "listening :\(port)"
                case .failed(let e): self?.status = "failed \(e.localizedDescription)"
                default: break
                }
            }
        }
        listener?.start(queue: queue)
        status = "starting :\(port)"
    }

    func stop() {
        connection?.cancel(); connection = nil
        listener?.cancel(); listener = nil
        decoder.reset()
        status = "stopped"
    }

    func sendText(_ text: String) {
        sendPayload(Data(text.utf8))
    }

    func sendPayload(_ payload: Data) {
        guard let connection else { return }
        let frame = FrameCodec.encode(payload)
        connection.send(content: frame, completion: .contentProcessed { _ in })
    }

    private func accept(_ conn: NWConnection) {
        connection?.cancel()
        connection = conn
        decoder.reset()
        conn.stateUpdateHandler = { [weak self] st in
            Task { @MainActor in
                switch st {
                case .ready:
                    self?.state?.onClientConnected()
                    self?.status = "client connected :\(self?.port ?? 0)"
                case .failed, .cancelled:
                    self?.state?.onClientDisconnected()
                    self?.status = "listening :\(self?.port ?? 0)"
                    self?.connection = nil
                default: break
                }
            }
        }
        conn.start(queue: queue)
        receive(on: conn)
        // Wire outbound
        state?.sendNotify = { [weak self] text in self?.sendText(text) }
        state?.sendBinary = { [weak self] data in self?.sendPayload(data) }
    }

    private func receive(on conn: NWConnection) {
        conn.receive(minimumIncompleteLength: 1, maximumLength: 64 * 1024) { [weak self] data, _, isComplete, error in
            Task { @MainActor in
                guard let self else { return }
                if let data, !data.isEmpty {
                    for frame in self.decoder.append(data) {
                        self.state?.handleInbound(frame)
                    }
                }
                if isComplete || error != nil {
                    conn.cancel()
                    return
                }
                self.receive(on: conn)
            }
        }
    }
}
