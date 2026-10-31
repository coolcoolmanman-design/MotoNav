import SwiftUI

/// 设备 Tab：加大圆屏连接卡 + 胎压轮胎图（对齐 IA mockups）。
struct DeviceConnectionCard: View {
    @ObservedObject var ble = BleLink.shared
    @ObservedObject var navi = NaviBridge.shared
    @ObservedObject var map = MapFeeder.shared
    @ObservedObject var settings = Settings.shared
    @ObservedObject private var connectAnim = ConnectAnimationCoordinator.shared
    var onDisplay: () -> Void
    var onMap: () -> Void
    var onSystem: () -> Void

    var body: some View {
        CardView {
            HStack(alignment: .top, spacing: 12) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("导航屏")
                        .font(.caption)
                        .foregroundColor(AppTheme.sub)
                    Text(ble.peripheralName ?? "VELO")
                        .font(.system(size: 28, weight: .bold))
                        .foregroundColor(AppTheme.text)
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)
                    Text("1.85B · 圆屏导航")
                        .font(.caption)
                        .foregroundColor(AppTheme.sub)
                    HStack(spacing: 6) {
                        Circle()
                            .fill(ble.isReady ? AppTheme.green : AppTheme.amber)
                            .frame(width: 8, height: 8)
                        Text(ble.isReady ? "已连接" : ble.status)
                            .font(.subheadline.weight(.medium))
                            .foregroundColor(ble.isReady ? AppTheme.green : AppTheme.sub)
                            .lineLimit(1)
                    }
                    .padding(.top, 2)
                    HStack(spacing: 18) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("设备电量").font(.caption2).foregroundColor(AppTheme.sub)
                            Text(ble.batteryPercent.map { "\($0)%" } ?? "—")
                                .font(.subheadline.weight(.semibold).monospacedDigit())
                                .foregroundColor(AppTheme.text)
                        }
                        VStack(alignment: .leading, spacing: 2) {
                            Text("存储卡").font(.caption2).foregroundColor(AppTheme.sub)
                            Text(ble.isReady ? "已就绪" : "—")
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(AppTheme.text)
                        }
                    }
                    .padding(.top, 4)
                    GlassChip(
                        title: ble.isReady ? "断开" : "连接",
                        icon: ble.isReady ? "bolt.slash" : "bolt.fill",
                        tint: ble.isReady ? nil : AppTheme.accent,
                        compact: true
                    ) {
                        if ble.isReady { ble.disconnect() }
                        else { connectAnim.connectManually() }
                    }
                    .padding(.top, 4)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                ZStack(alignment: .bottom) {
                    Image("GroundShadow")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 150, height: 36)
                        .offset(y: 8)
                        .opacity(0.85)
                    Image("DevicePuckHero")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 148, height: 168)
                        .shadow(color: .black.opacity(0.12), radius: 8, y: 4)
                }
                .frame(width: 150, height: 180)
            }

            if navi.navigating {
                Text("导航中：\(navi.uiDir) \(navi.uiDist) 米 · \(navi.uiSpeed) km/h")
                    .font(.footnote)
                    .foregroundColor(AppTheme.accent)
            }
            Text("小地图：\(map.shortStatus) · \(Settings.themeName(settings.theme))")
                .font(.caption2)
                .foregroundColor(AppTheme.sub)

            Divider().background(AppTheme.stroke)

            HStack(spacing: 0) {
                deviceQuickLink("显示", "sun.max", onDisplay)
                Divider().frame(height: 28).background(AppTheme.stroke)
                deviceQuickLink("地图", "map", onMap)
                Divider().frame(height: 28).background(AppTheme.stroke)
                deviceQuickLink("系统", "gearshape", onSystem)
            }
        }
    }

    private func deviceQuickLink(_ title: String, _ icon: String, _ action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 6) {
                Image(systemName: icon).font(.system(size: 14, weight: .semibold))
                Text(title).font(.subheadline.weight(.medium))
            }
            .foregroundColor(AppTheme.accent)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 6)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

/// 胎压区：前/后轮大卡 + 轮胎图；阈值 / 配对收入二级。
struct DeviceTpmsSection: View {
    @ObservedObject var tpms = TpmsManager.shared
    var onSettings: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("胎压监测")
                    .font(.system(size: 20, weight: .bold))
                    .foregroundColor(AppTheme.text)
                Spacer()
                Button(action: onSettings) {
                    Image(systemName: "slider.horizontal.3")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundColor(AppTheme.sub)
                        .frame(width: 32, height: 32)
                        .background(Circle().fill(AppTheme.sunken))
                }
                .buttonStyle(.plain)
            }
            HStack(spacing: 10) {
                tireCard(label: "前轮", reading: tpms.front)
                tireCard(label: "后轮", reading: tpms.rear)
            }
            Text("已配对 \(tpms.pairedCount)/2 · 扫描中 · 最近心跳 \(tpms.lastAdvs) 条")
                .font(.caption2)
                .foregroundColor(AppTheme.sub)
        }
    }

    private func tireCard(label: String, reading: TpmsManager.Reading?) -> some View {
        let status = tpms.status(reading)
        let warn = status.map { $0.0 != "正常" } ?? false
        // mockup 用 bar；内部仍存 kPa（1 bar ≈ 100 kPa）
        let barText: String = {
            guard let r = reading else { return "--" }
            return String(format: "%.1f", Double(r.kpa) / 100.0)
        }()
        return VStack(alignment: .leading, spacing: 8) {
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
            HStack(alignment: .bottom, spacing: 6) {
                VStack(alignment: .leading, spacing: 2) {
                    HStack(alignment: .firstTextBaseline, spacing: 3) {
                        Text(barText)
                            .font(.system(size: 28, weight: .bold, design: .rounded).monospacedDigit())
                            .foregroundColor(AppTheme.text)
                        Text("bar").font(.caption).foregroundColor(AppTheme.sub)
                    }
                    Rectangle()
                        .fill(AppTheme.text.opacity(0.85))
                        .frame(width: 36, height: 3)
                        .cornerRadius(1.5)
                    if let r = reading {
                        Text("\(r.tempC)°C").font(.caption).foregroundColor(AppTheme.sub)
                    } else {
                        Text("等待数据").font(.caption2).foregroundColor(AppTheme.sub)
                    }
                }
                Spacer(minLength: 0)
                Image("TireArt")
                    .resizable()
                    .scaledToFit()
                    .frame(width: 72, height: 72)
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 16).fill(AppTheme.card))
        .overlay(
            RoundedRectangle(cornerRadius: 16)
                .stroke(warn ? (status?.1.opacity(0.85) ?? AppTheme.stroke) : AppTheme.stroke,
                        lineWidth: warn ? 1.5 : 1)
        )
    }
}
