import Foundation
import Combine

/// AirPods 式连接动画：首次 / 手动 / 自动连接均弹出。
final class ConnectAnimationCoordinator: ObservableObject {
    static let shared = ConnectAnimationCoordinator()
    private static let seenKey = "connect_anim_first_done"

    enum Kind { case first, manual, auto }

    @Published var isPresented = false
    @Published var kind: Kind = .manual
    @Published var phase: Phase = .idle
    @Published var statusText = "正在连接…"
    @Published var showBattery = false

    enum Phase { case idle, spinning, connected }

    /// 动画最短时长（与 mock 一致：约 3.2s 转完再定格）
    let spinSeconds: TimeInterval = 3.2
    private var spinStartedAt: Date?
    private var readyWatch: AnyCancellable?
    private var finishWork: DispatchWorkItem?

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

    var isFirstDone: Bool {
        get { UserDefaults.standard.bool(forKey: Self.seenKey) }
        set { UserDefaults.standard.set(newValue, forKey: Self.seenKey) }
    }

    /// 用户点「连接」：弹 sheet + 发起 BLE
    func connectManually() {
        kind = isFirstDone ? .manual : .first
        presentAndConnect()
    }

    /// App 启动 / 静默重连：同样弹动画
    func connectAutomatically() {
        kind = isFirstDone ? .auto : .first
        presentAndConnect()
    }

    /// 设置页可重置「首次」标记，方便反复看动画
    func resetFirstFlag() { isFirstDone = false }

    func dismiss() {
        finishWork?.cancel()
        readyWatch?.cancel()
        readyWatch = nil
        isPresented = false
        phase = .idle
        showBattery = false
        statusText = "正在连接…"
    }

    private func presentAndConnect() {
        // 已在展示中则只确保在连，避免叠两层
        if isPresented {
            BleLink.shared.connect()
            return
        }
        finishWork?.cancel()
        readyWatch?.cancel()
        isPresented = true
        phase = .spinning
        showBattery = false
        statusText = kind == .auto ? "正在重新连接…" : "正在连接…"
        spinStartedAt = Date()
        BleLink.shared.connect()

        readyWatch = BleLink.shared.$isReady
            .receive(on: DispatchQueue.main)
            .sink { [weak self] ready in
                guard let self, self.isPresented, ready else { return }
                self.scheduleConnected()
            }
        if BleLink.shared.isReady { scheduleConnected() }
    }

    private func scheduleConnected() {
        let started = spinStartedAt ?? Date()
        let remain = max(0, spinSeconds - Date().timeIntervalSince(started))
        finishWork?.cancel()
        let work = DispatchWorkItem { [weak self] in
            guard let self, self.isPresented else { return }
            self.phase = .connected
            self.statusText = self.kind == .auto ? "已重新连接" : "已连接"
            self.showBattery = true
            if self.kind == .first { self.isFirstDone = true }
            // 稍停再自动收起，方便看清电量
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { [weak self] in
                guard let self, self.isPresented, self.phase == .connected else { return }
                self.dismiss()
            }
        }
        finishWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + remain, execute: work)
    }
}
