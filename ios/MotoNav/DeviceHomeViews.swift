import SwiftUI

/// 圆屏 Tab：连接卡（产品正视 PNG）+ 圆屏显示 / 圆屏设置入口 + 胎压全字段。
/// 第一卡布局对齐 mockup b7：无「圆屏导航」；电量 / 连接 / 断开同行；hero 略小并相对「小地图」带居中左移。
struct DeviceConnectionCard<DisplayDest: View, SystemDest: View>: View {
    @ObservedObject var ble = BleLink.shared
    @ObservedObject var navi = NaviBridge.shared
    @ObservedObject var map = MapFeeder.shared
    @ObservedObject var settings = Settings.shared
    @ObservedObject private var connectAnim = ConnectAnimationCoordinator.shared
    @ViewBuilder var displayDestination: () -> DisplayDest
    @ViewBuilder var systemDestination: () -> SystemDest

    var body: some View {
        CardView {
            HStack(alignment: .center, spacing: 6) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("导航屏")
                        .font(.caption)
                        .foregroundColor(AppTheme.sub)
                    Text(ble.peripheralName ?? "VELO")
                        .font(.system(size: 28, weight: .bold))
                        .foregroundColor(AppTheme.text)
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)

                    // b7：设备电量 + 连接信息 + 断开/连接 同一行
                    HStack(spacing: 8) {
                        Text("设备电量 \(ble.batteryPercent.map { "\($0)%" } ?? "—")")
                            .font(.caption.weight(.medium).monospacedDigit())
                            .foregroundColor(AppTheme.text)
                            .lineLimit(1)
                            .layoutPriority(1)
                        HStack(spacing: 5) {
                            Circle()
                                .fill(ble.isReady ? AppTheme.green : AppTheme.amber)
                                .frame(width: 8, height: 8)
                            Text(ble.isReady ? "已连接" : "未连接")
                                .font(.caption.weight(.medium))
                                .foregroundColor(ble.isReady ? AppTheme.green : AppTheme.sub)
                                .lineLimit(1)
                        }
                        Spacer(minLength: 4)
                        GlassChip(
                            title: ble.isReady ? "断开" : "连接",
                            tint: ble.isReady ? nil : AppTheme.accent,
                            compact: true
                        ) {
                            if ble.isReady { ble.disconnect() }
                            else { connectAnim.connectManually() }
                        }
                    }

                    if navi.navigating {
                        Text("导航中：\(navi.uiDir) \(navi.uiDist) 米 · \(navi.uiSpeed) km/h")
                            .font(.footnote)
                            .foregroundColor(AppTheme.accent)
                    }
                    Text("小地图：\(map.shortStatus) · \(Settings.themeName(settings.theme))")
                        .font(.caption2)
                        .foregroundColor(AppTheme.sub)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                // b7 / v6：略小于 0.15.23 大图；相对含「小地图」的左栏垂直居中，并左移收中缝
                ZStack(alignment: .bottom) {
                    Image("GroundShadow")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 126, height: 28)
                        .offset(y: 6)
                        .opacity(0.75)
                    Image("DevicePuckHero")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 128, height: 144)
                }
                .frame(width: 132, height: 158)
                .offset(x: -10)
            }

            Divider().opacity(0.35)

            HStack(spacing: 0) {
                NavigationLink(destination: displayDestination()) {
                    deviceQuickLinkLabel("圆屏显示", "sun.max")
                }
                .buttonStyle(.plain)
                Divider().frame(height: 28).opacity(0.35)
                NavigationLink(destination: systemDestination()) {
                    deviceQuickLinkLabel("圆屏设置", "gearshape")
                }
                .buttonStyle(.plain)
            }
        }
    }

    private func deviceQuickLinkLabel(_ title: String, _ icon: String) -> some View {
        HStack(spacing: 6) {
            Image(systemName: icon).font(.system(size: 14, weight: .semibold))
            Text(title).font(.subheadline.weight(.medium))
        }
        .foregroundColor(AppTheme.accent)
        .frame(maxWidth: .infinity)
        .padding(.vertical, 6)
        .contentShape(Rectangle())
    }
}

/// 胎压区：前/后轮 + 缩小轮胎图 + 阈值 / 配对等完整字段。
struct DeviceTpmsSection: View {
    @ObservedObject var tpms = TpmsManager.shared

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("胎压监测")
                .font(.system(size: 20, weight: .bold))
                .foregroundColor(AppTheme.text)
            HStack(spacing: 10) {
                tireCard(label: "前轮", reading: tpms.front)
                tireCard(label: "后轮", reading: tpms.rear)
            }
            CardView {
                tpmsThreshold("低压报警", value: tpms.lowKpa, unit: "kPa") {
                    tpms.lowKpa = max(100, tpms.lowKpa - 10)
                } up: {
                    tpms.lowKpa = min(300, tpms.lowKpa + 10)
                }
                tpmsThreshold("高温报警", value: tpms.highTemp, unit: "°C") {
                    tpms.highTemp = max(40, tpms.highTemp - 5)
                } up: {
                    tpms.highTemp = min(110, tpms.highTemp + 5)
                }
                Text("越限时读数标红；气嘴静止不广播时保持最后一次读数")
                    .font(.caption2).foregroundColor(AppTheme.sub)
                tpmsPairRow("前轮气嘴", id: tpms.frontId)
                tpmsPairRow("后轮气嘴", id: tpms.rearId)
                HStack(spacing: 10) {
                    GlassChip(title: "重新配对", icon: "arrow.triangle.2.circlepath", tint: AppTheme.accent, compact: true) { tpms.clearPairing() }
                    GlassChip(title: "发送测试帧", foreground: AppTheme.sub, compact: true) { tpms.sendTest() }
                }
                Text("扫描中 · 最近心跳 \(tpms.lastAdvs) 条广播 · 已配对 \(tpms.pairedCount)/2")
                    .font(.caption2).foregroundColor(AppTheme.sub)
            }
        }
    }

    private func tireCard(label: String, reading: TpmsManager.Reading?) -> some View {
        let status = tpms.status(reading)
        let warn = status.map { $0.0 != "正常" } ?? false
        let barText: String = {
            guard let r = reading else { return "--" }
            return String(format: "%.1f", Double(r.kpa) / 100.0)
        }()
        return VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(label).font(.caption).foregroundColor(AppTheme.sub)
                Spacer()
                if let s = status {
                    Text(s.0)
                        .font(.caption2.weight(.semibold))
                        .foregroundColor(s.1)
                        .padding(.horizontal, 7).padding(.vertical, 2)
                        .background(Capsule().fill(s.1.opacity(0.14)))
                }
            }
            HStack(alignment: .center, spacing: 8) {
                VStack(alignment: .leading, spacing: 5) {
                    // 压力 / 温度 / 电量：三项并列展示（Reading 已有 kpa/tempC/battPct）
                    HStack(alignment: .firstTextBaseline, spacing: 3) {
                        Text(barText)
                            .font(.system(size: 24, weight: .bold, design: .rounded).monospacedDigit())
                            .foregroundColor(AppTheme.text)
                        Text("bar").font(.caption).foregroundColor(AppTheme.sub)
                    }
                    if let r = reading {
                        tpmsMetricRow(icon: "thermometer.medium", text: "\(r.tempC)°C")
                        tpmsMetricRow(icon: batterySymbol(r.battPct), text: "\(r.battPct)%")
                    } else {
                        Text("等待压力 · 温度 · 电量")
                            .font(.caption2).foregroundColor(AppTheme.sub)
                    }
                }
                Spacer(minLength: 0)
                Image("TireArt")
                    .resizable()
                    .scaledToFit()
                    .frame(width: 44, height: 44)
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 16).fill(AppTheme.card))
        .overlay(
            RoundedRectangle(cornerRadius: 16)
                .stroke(warn ? (status?.1.opacity(0.85) ?? Color.clear) : Color.clear,
                        lineWidth: warn ? 1.5 : 0)
        )
    }

    private func tpmsMetricRow(icon: String, text: String) -> some View {
        HStack(spacing: 4) {
            Image(systemName: icon)
                .font(.caption2.weight(.semibold))
                .foregroundColor(AppTheme.sub)
                .frame(width: 14, alignment: .center)
            Text(text)
                .font(.caption.weight(.medium).monospacedDigit())
                .foregroundColor(AppTheme.text)
        }
    }

    private func batterySymbol(_ pct: Int) -> String {
        switch pct {
        case 75...100: return "battery.100"
        case 50..<75: return "battery.50"
        case 25..<50: return "battery.25"
        default: return "battery.0"
        }
    }

    private func tpmsThreshold(_ label: String, value: Int, unit: String,
                               down: @escaping () -> Void, up: @escaping () -> Void) -> some View {
        HStack {
            Text(label).foregroundColor(AppTheme.text)
            Spacer()
            Button(action: down) {
                Text("−").font(.subheadline.weight(.semibold))
                    .foregroundColor(AppTheme.text)
                    .frame(width: 28, height: 28)
                    .background(Capsule().fill(AppTheme.sunken))
            }.buttonStyle(.plain)
            Text("\(value) \(unit)")
                .font(.subheadline.weight(.semibold))
                .foregroundColor(AppTheme.text)
                .frame(minWidth: 74)
            Button(action: up) {
                Text("+").font(.subheadline.weight(.semibold))
                    .foregroundColor(AppTheme.text)
                    .frame(width: 28, height: 28)
                    .background(Capsule().fill(AppTheme.sunken))
            }.buttonStyle(.plain)
        }
    }

    private func tpmsPairRow(_ label: String, id: String) -> some View {
        HStack(spacing: 6) {
            Circle().fill(id.isEmpty ? AppTheme.stroke : AppTheme.green).frame(width: 6, height: 6)
            Text(label).font(.subheadline).foregroundColor(AppTheme.text)
            if !id.isEmpty {
                Text("\(id.prefix(8))…").font(.caption2).foregroundColor(AppTheme.sub)
            }
            Spacer()
            Text(id.isEmpty ? "未配对" : "已配对")
                .font(.caption)
                .foregroundColor(id.isEmpty ? AppTheme.sub : AppTheme.green)
        }
    }
}
