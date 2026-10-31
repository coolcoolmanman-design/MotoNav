import Foundation
import Combine

/// AirPods 式连接动画：每次手动/自动连接成功（含 BLE 静默重连）都弹出，不只首次或圆屏 Tab。
final class ConnectAnimationCoordinator: ObservableObject {
    static let shared = ConnectAnimationCoordinator()

    enum Kind { case first, manual, auto }

    @Published var isPresented = false
    @Published var kind: Kind = .manual
    @Published var phase: Phase = .idle
    @Published var statusText = "正在连接圆屏…"
    @Published var showBattery = false

    enum Phase { case idle, spinning, connected }

    let spinSeconds: TimeInterval = 3.2
    private var spinStartedAt: Date?
    private var readyWatch: AnyCancellable?
    private var linkWatch: AnyCancellable?
    private var finishWork: DispatchWorkItem?
    private var everConnected = false
    /// 已为当前「已连接」周期弹过动画，避免 isReady 抖动重复弹
    private var celebratedThisLink = false

    var deviceTitle: String {
        BleLink.shared.peripheralName ?? "MotoNav"
    }

    var footerHint: String {
        switch kind {
        case .first: return "首次配对 · BLE"
        case .manual: return "手动连接 · BLE"
        case .auto: return "自动重连 · BLE"
        }
    }

    private init() {
        // 任意路径让链路就绪（启动自动连 / 断线重连 / 手动）都走一次动画
        linkWatch = BleLink.shared.$isReady
            .receive(on: DispatchQueue.main)
            .sink { [weak self] ready in
                guard let self else { return }
                if !ready {
                    self.celebratedThisLink = false
                    return
                }
                self.notifyLinkBecameReady()
            }
    }

    /// 用户点「连接」：断开后再连也整段重播动画
    func connectManually() {
        kind = everConnected ? .manual : .first
        presentAndConnect(forceRestart: true)
    }

    /// App 启动主动拉起连接：同样弹动画
    func connectAutomatically() {
        kind = everConnected ? .auto : .first
        presentAndConnect(forceRestart: true)
    }

    /// 兼容旧设置页入口（不再用于跳过动画）
    func resetFirstFlag() { everConnected = false }

    /// BLE 任意路径变就绪（含静默重连）：弹同款 sheet；已在展示则只推进到已连接态，不重复叠层
    func notifyLinkBecameReady() {
        if isPresented {
            scheduleConnected()
            return
        }
        guard !celebratedThisLink else { return }
        celebratedThisLink = true
        kind = everConnected ? .auto : .first
        presentCelebrationOnly()
    }

    func dismiss() {
        finishWork?.cancel()
        readyWatch?.cancel()
        readyWatch = nil
        isPresented = false
        phase = .idle
        showBattery = false
        statusText = "正在连接圆屏…"
    }

    private func presentAndConnect(forceRestart: Bool) {
        if isPresented && !forceRestart {
            BleLink.shared.connect()
            return
        }
        finishWork?.cancel()
        readyWatch?.cancel()
        celebratedThisLink = BleLink.shared.isReady
        isPresented = true
        phase = .spinning
        showBattery = false
        statusText = kind == .auto ? "正在重新连接圆屏…" : "正在连接圆屏…"
        spinStartedAt = Date()
        BleLink.shared.connect()

        readyWatch = BleLink.shared.$isReady
            .receive(on: DispatchQueue.main)
            .sink { [weak self] ready in
                guard let self, self.isPresented, ready else { return }
                self.celebratedThisLink = true
                self.scheduleConnected()
            }
        if BleLink.shared.isReady {
            celebratedThisLink = true
            scheduleConnected()
        }
    }

    /// 链路已就绪时只播动画（不再调 connect，避免重入握手）
    private func presentCelebrationOnly() {
        finishWork?.cancel()
        readyWatch?.cancel()
        readyWatch = nil
        isPresented = true
        phase = .spinning
        showBattery = false
        statusText = kind == .auto ? "正在重新连接圆屏…" : "正在连接圆屏…"
        // 自动连上：略缩短空转，尽快进「已连接」
        spinStartedAt = Date().addingTimeInterval(-(spinSeconds * 0.45))
        scheduleConnected()
    }

    private func scheduleConnected() {
        let started = spinStartedAt ?? Date()
        let remain = max(0, spinSeconds - Date().timeIntervalSince(started))
        finishWork?.cancel()
        let work = DispatchWorkItem { [weak self] in
            guard let self, self.isPresented else { return }
            self.phase = .connected
            self.statusText = self.kind == .auto ? "已重新连接圆屏" : "已连接圆屏"
            self.showBattery = true
            self.everConnected = true
            self.celebratedThisLink = true
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { [weak self] in
                guard let self, self.isPresented, self.phase == .connected else { return }
                self.dismiss()
            }
        }
        finishWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + remain, execute: work)
    }
}
