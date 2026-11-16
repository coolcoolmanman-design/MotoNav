import SwiftUI

/// 我的 → 通用 → 电量管理（P0：省电 / 10% 自动 / 插电退 / 折线 / App 内横幅相关设置 / 高级短按息屏）。
struct BatteryManageView: View {
    @ObservedObject var settings = Settings.shared
    @ObservedObject var ble = BleLink.shared

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            statusCard
            chartCard
            powerCard
            autoCard
            linksCard
            placeholderCard
        }
    }

    private var statusCard: some View {
        CardView {
            Text("设备电量").foregroundColor(AppTheme.text).bold()
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                Text(ble.batteryPercent.map { "\($0)%" } ?? "—")
                    .font(.system(size: 36, weight: .semibold, design: .rounded))
                    .foregroundColor(AppTheme.text)
                if ble.screenCharging {
                    Text("充电中")
                        .font(.caption.weight(.semibold))
                        .foregroundColor(.white)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .background(Capsule().fill(AppTheme.accent))
                }
                Spacer()
            }
            if let mv = ble.batteryMillivolts {
                Text("电压 \(mv) mV").font(.caption).foregroundColor(AppTheme.sub)
            }
            Text(ble.isReady ? "数据来自圆屏实时上报" : "未连接 · 显示上次读数")
                .font(.caption2).foregroundColor(AppTheme.sub)
        }
    }

    private var chartCard: some View {
        CardView {
            Text("近几小时").foregroundColor(AppTheme.text).bold()
            Text("示意级 · 仅连接期间有数据；断连处留白")
                .font(.caption2).foregroundColor(AppTheme.sub)
            if ble.batteryHistory.isEmpty {
                Text("暂无采样").font(.footnote).foregroundColor(AppTheme.sub)
                    .frame(maxWidth: .infinity, minHeight: 120, alignment: .center)
            } else {
                BatterySparkline(points: ble.batteryHistory.map(\.percent))
                    .frame(height: 120)
            }
        }
    }

    private var powerCard: some View {
        CardView {
            Text("省电模式").foregroundColor(AppTheme.text).bold()
            Toggle("省电模式", isOn: $settings.lowPowerOn)
                .foregroundColor(AppTheme.text).tint(AppTheme.accent)
            Toggle("低于 10% 自动开启", isOn: $settings.lowPowerAuto)
                .foregroundColor(AppTheme.text).tint(AppTheme.accent)
            Text("插电后自动退出（固定开启）")
                .font(.footnote).foregroundColor(AppTheme.sub)
            Text("开启后：降亮上限、关闭动态背景、中央动画冻帧。导航/巡航保活不受影响。")
                .font(.caption2).foregroundColor(AppTheme.sub)
            Text(settings.lowPowerStatus).font(.caption).foregroundColor(AppTheme.sub)
        }
    }

    private var autoCard: some View {
        CardView {
            Text("自动息屏与关机").foregroundColor(AppTheme.text).bold()
            Text("待机屏保：约 1 分钟（现行默认）")
                .font(.footnote).foregroundColor(AppTheme.sub)
            Text("闲置关机（仅电池）：约 5 分钟")
                .font(.footnote).foregroundColor(AppTheme.sub)
            Toggle("电池供电时允许短按息屏", isOn: $settings.pwrBatOffAllow)
                .foregroundColor(AppTheme.text).tint(AppTheme.accent)
            Text("默认关闭以防骑行误触；USB 供电时短按息屏始终可用。")
                .font(.caption2).foregroundColor(AppTheme.sub)
            Text(settings.pwrBatOffStatus).font(.caption).foregroundColor(AppTheme.sub)
        }
    }

    private var linksCard: some View {
        CardView {
            Text("显示").foregroundColor(AppTheme.text).bold()
            Text("电量图标显隐 → 圆屏设置 · 六槽「电量」")
                .font(.footnote).foregroundColor(AppTheme.sub)
            Text("亮度 → 圆屏设置 · 方向与亮度")
                .font(.footnote).foregroundColor(AppTheme.sub)
        }
    }

    private var placeholderCard: some View {
        CardView {
            Text("电池健康").foregroundColor(AppTheme.sub)
            Text("当前硬件不支持").font(.footnote).foregroundColor(AppTheme.sub.opacity(0.8))
            Divider().opacity(0.3)
            Text("优化充电").foregroundColor(AppTheme.sub)
            Text("当前硬件不支持").font(.footnote).foregroundColor(AppTheme.sub.opacity(0.8))
        }
        .opacity(0.55)
    }
}

/// App 内低电横幅（非系统推送）。挂在根 ContentView 顶部。
struct LowBatteryBanner: View {
    @ObservedObject var ble = BleLink.shared
    @ObservedObject var settings = Settings.shared
    @State private var dismissedAt: Date?

    var body: some View {
        if shouldShow {
            HStack(spacing: 10) {
                Image(systemName: "battery.25")
                    .foregroundColor(.white)
                VStack(alignment: .leading, spacing: 2) {
                    Text("圆屏电量低（\(ble.batteryPercent ?? 0)%）")
                        .font(.subheadline.weight(.semibold)).foregroundColor(.white)
                    Text("建议开启省电模式")
                        .font(.caption2).foregroundColor(.white.opacity(0.9))
                }
                Spacer()
                Button("开启省电") {
                    settings.lowPowerOn = true
                    dismissedAt = Date()
                }
                .font(.caption.weight(.semibold))
                .foregroundColor(AppTheme.accent)
                .padding(.horizontal, 10).padding(.vertical, 6)
                .background(Capsule().fill(Color.white))
                Button("稍后") { dismissedAt = Date() }
                    .font(.caption).foregroundColor(.white.opacity(0.9))
            }
            .padding(12)
            .background(RoundedRectangle(cornerRadius: 14, style: .continuous).fill(Color(red: 0.75, green: 0.18, blue: 0.15)))
            .padding(.horizontal, 12)
        }
    }

    private var shouldShow: Bool {
        guard ble.isReady, let p = ble.batteryPercent, p <= 10, !ble.screenCharging else { return false }
        if settings.lowPowerOn { return false }
        if let d = dismissedAt, Date().timeIntervalSince(d) < 600 { return false }
        return true
    }
}

struct BatterySparkline: View {
    let points: [Int]
    var body: some View {
        GeometryReader { geo in
            let w = geo.size.width, h = geo.size.height
            let n = max(points.count, 2)
            Path { path in
                for (i, p) in points.enumerated() {
                    let x = w * CGFloat(i) / CGFloat(n - 1)
                    let y = h * (1 - CGFloat(max(0, min(100, p))) / 100)
                    if i == 0 { path.move(to: CGPoint(x: x, y: y)) }
                    else { path.addLine(to: CGPoint(x: x, y: y)) }
                }
            }
            .stroke(AppTheme.accent, lineWidth: 2)
        }
        .background(RoundedRectangle(cornerRadius: 12).fill(Color.black.opacity(0.85)))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}
