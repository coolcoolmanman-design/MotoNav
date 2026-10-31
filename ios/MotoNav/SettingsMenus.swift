import SwiftUI

/// 「我的」顶层分组：二级菜单入口（少滚动）。
enum SettingsHubItem: String, Identifiable, CaseIterable {
    case general, displayConnect, tpms, mapNavi, about
    var id: String { rawValue }
    var title: String {
        switch self {
        case .general: return "通用"
        case .displayConnect: return "圆屏与连接"
        case .tpms: return "胎压监测 TPMS"
        case .mapNavi: return "地图与导航"
        case .about: return "关于"
        }
    }
    var subtitle: String {
        switch self {
        case .general: return "主题与语言"
        case .displayConnect: return "固件与显示"
        case .tpms: return "轮胎状态"
        case .mapNavi: return "路径设置"
        case .about: return "系统信息"
        }
    }
    var icon: String {
        switch self {
        case .general: return "gearshape"
        case .displayConnect: return "circle.circle"
        case .tpms: return "gauge.with.dots.needle.33percent"
        case .mapNavi: return "map"
        case .about: return "info.circle"
        }
    }
}

struct SettingsHubList: View {
    var onSelect: (SettingsHubItem) -> Void
    var body: some View {
        VStack(spacing: 0) {
            ForEach(Array(SettingsHubItem.allCases.enumerated()), id: \.element.id) { idx, item in
                Button { onSelect(item) } label: {
                    HStack(spacing: 12) {
                        Image(systemName: item.icon)
                            .font(.system(size: 17, weight: .semibold))
                            .foregroundColor(AppTheme.accent)
                            .frame(width: 28)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(item.title)
                                .font(.body.weight(.semibold))
                                .foregroundColor(AppTheme.text)
                            Text(item.subtitle)
                                .font(.caption)
                                .foregroundColor(AppTheme.sub)
                        }
                        Spacer()
                        Image(systemName: "chevron.right")
                            .font(.caption.weight(.semibold))
                            .foregroundColor(AppTheme.sub.opacity(0.7))
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 14)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                if idx < SettingsHubItem.allCases.count - 1 {
                    Divider().padding(.leading, 56).background(AppTheme.stroke)
                }
            }
        }
        .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))
        .overlay(RoundedRectangle(cornerRadius: 18).stroke(AppTheme.stroke, lineWidth: 1))
    }
}

/// 圆屏与连接二级页的状态摘要（显示/固件细则由 ContentView 卡片提供）。
struct DisplayConnectStatusRows: View {
    @ObservedObject var ble = BleLink.shared
    var body: some View {
        CardView {
            Text("状态").font(.caption).foregroundColor(AppTheme.sub)
            statusRow("连接状态", ble.isReady ? "已连接" : "未连接",
                      valueColor: ble.isReady ? AppTheme.green : AppTheme.amber)
            statusRow("设备电量", ble.batteryPercent.map { "\($0)%" } ?? "—")
            statusRow("存储卡", ble.isReady ? "已就绪" : "—")
        }
    }
    private func statusRow(_ title: String, _ value: String, valueColor: Color = AppTheme.text) -> some View {
        HStack {
            Text(title).foregroundColor(AppTheme.text)
            Spacer()
            Text(value).foregroundColor(valueColor)
        }
        .font(.body)
    }
}
