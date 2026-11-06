import SwiftUI

struct ContentView: View {
    @StateObject private var state = SimState()
    @StateObject private var server = TcpServer()

    var body: some View {
        HStack(alignment: .top, spacing: 20) {
            RoundCanvas(state: state)
                .padding(.leading, 8)

            VStack(alignment: .leading, spacing: 12) {
                Text("Mac RoundSim")
                    .font(.title2.bold())
                Text("TCP \(server.status)")
                    .font(.system(.caption, design: .monospaced))
                    .foregroundStyle(.secondary)

                HStack {
                    Button(server.status.hasPrefix("listening") || server.status.hasPrefix("client") ? "Restart" : "Listen") {
                        wireAndStart()
                    }
                    Button("Stop") { server.stop() }
                    Toggle("Bezel", isOn: $state.showBezel)
                    Toggle("Sim TOUCH_FLIP raw", isOn: $state.simulateDeviceRawTouch)
                }

                Picker("Page", selection: $state.page) {
                    ForEach(SimPage.allCases, id: \.self) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)

                GroupBox("Manual inject (# commands)") {
                    TextEditor(text: $state.injectText)
                        .font(.system(.body, design: .monospaced))
                        .frame(minHeight: 72, maxHeight: 120)
                    HStack {
                        Button("Send") { state.injectManual() }
                        Button("Arrive demo") {
                            state.injectText = "#O12340/28/67\nA,0"
                            state.injectManual()
                        }
                        Button("Music playing") {
                            state.injectText = "#Z,1,1,7\n#RC,1"
                            state.injectManual()
                        }
                        Button("Nav HUD") {
                            state.injectText = "#m,1\n#v,48\nR,120\n#e,2400,1830,0"
                            state.injectManual()
                        }
                    }
                }

                GroupBox("Log") {
                    ScrollView {
                        LazyVStack(alignment: .leading, spacing: 2) {
                            ForEach(Array(state.logLines.enumerated()), id: \.offset) { _, line in
                                Text(line).font(.system(size: 11, design: .monospaced))
                            }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .frame(minHeight: 180)
                }

                Text("Port \(TcpServer.defaultPort) · frame = u32 LE len + BLE payload · 466×466 clip")
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
            }
            .frame(minWidth: 360, maxWidth: 420)
            .padding(.trailing, 12)
        }
        .padding(12)
        .frame(minWidth: 980, minHeight: 620)
        .onAppear { wireAndStart() }
        .onDisappear { server.stop() }
    }

    private func wireAndStart() {
        server.state = state
        state.sendNotify = { [weak server] text in server?.sendText(text) }
        state.sendBinary = { [weak server] data in server?.sendPayload(data) }
        server.start()
    }
}
