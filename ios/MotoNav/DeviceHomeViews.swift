import SwiftUI
import UIKit

/// 圆屏 Tab：连接卡（产品正视 PNG）+ 圆屏显示 / 圆屏设置入口 + 胎压全字段。
/// 第一卡：b7 无「圆屏导航」+ 电量/连接/CTA 同行；0.15.33 标题同「胎压监测」主色字号，CTA 略左、设备图仅右移（不改 DevicePuckHero 位图）。
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
                        .font(.system(size: 20, weight: .bold))
                        .foregroundColor(AppTheme.text)
                    Text(ble.peripheralName ?? "VELO")
                        .font(.system(size: 28, weight: .bold))
                        .foregroundColor(AppTheme.text)
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)

                    // b7：设备电量 + 连接信息 + 断开/连接 同一行；CTA 略左贴状态
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
                        GlassChip(
                            title: ble.isReady ? "断开" : "连接",
                            tint: ble.isReady ? nil : AppTheme.accent,
                            compact: true
                        ) {
                            if ble.isReady { ble.disconnect() }
                            else { connectAnim.connectManually() }
                        }
                        .offset(x: -6)
                        Spacer(minLength: 0)
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

                // 0.15.35：软椭圆接触影（禁方形阴影板）+ 设备略左移加大右侧留白；不换 DevicePuckHero 位图
                ZStack(alignment: .bottom) {
                    Image("GroundShadow")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 118, height: 26)
                        .offset(y: 4)
                        .opacity(0.55)
                        .blendMode(.multiply)
                    Image("DevicePuckHero")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 128, height: 144)
                }
                .frame(width: 132, height: 158)
                .offset(x: -4)
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

/// 电子吧唧卡：单图；「选择图片」≠「上传图片」；自选自由宽高比裁切后居中方裁 466×466；bulk type 7 ver=1。
/// 部署目标 iOS 15：用 UIImagePickerController，不用 PhotosPicker / iOS 17 onChange。
struct DeviceBadgeCard: View {
    @ObservedObject var badge = BadgeBridge.shared
    @ObservedObject var ble = BleLink.shared
    @ObservedObject var btext = BadgeTextBridge.shared
    @State private var showPicker = false
    @State private var showCrop = false

    var body: some View {
        CardView {
            HStack(alignment: .top, spacing: 14) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("电子吧唧")
                        .font(.system(size: 20, weight: .bold))
                        .foregroundColor(AppTheme.text)
                    // musicA.18 显示模式：静图 / 文字滚动
                    Picker("模式", selection: $btext.textMode) {
                        Text("图片").tag(false)
                        Text("文字滚动").tag(true)
                    }
                    .pickerStyle(.segmented)
                    .padding(.bottom, 2)

                    if btext.textMode {
                        badgeTextSection
                    } else {
                        badgeImageSection
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                badgePreviewRing
            }
        }
        .sheet(isPresented: $showPicker) {
            BadgeImagePicker { img in
                showPicker = false
                if let img { badge.selectImage(img) }
            }
            .ignoresSafeArea()
        }
        .sheet(isPresented: $showCrop) {
            if let source = badge.sourceImage {
                BadgeCropEditor(
                    source: source,
                    initialMode: badge.cropMode,
                    initialCrop: badge.normalizedCrop
                        ?? (badge.cropMode == .manualRegion
                            ? BadgeBridge.defaultFreeCrop(for: source)
                            : BadgeBridge.defaultNormalizedCrop(for: source)),
                    onCancel: { showCrop = false },
                    onDone: { mode, norm in
                        showCrop = false
                        badge.applyCrop(mode: mode, normalizedCrop: norm)
                    }
                )
            } else {
                Text("无可裁切图片").padding()
            }
        }
    }

    /// musicA.18 文字滚动：输入 + 字号 + 颜色 + 上传。
    private var badgeTextSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("圆屏横向滚动显示，支持中文")
                .font(.footnote)
                .foregroundColor(AppTheme.sub)
            TextField("输入滚动文字…", text: $btext.badgeText)
                .font(.subheadline)
                .padding(10)
                .background(RoundedRectangle(cornerRadius: 10, style: .continuous).fill(AppTheme.sunken))
                .foregroundColor(AppTheme.text)
            HStack(spacing: 8) {
                Text("字号")
                    .font(.footnote)
                    .foregroundColor(AppTheme.sub)
                ForEach(0..<5, id: \.self) { i in
                    Button {
                        btext.textSizeIdx = i
                    } label: {
                        Text(BadgeTextBridge.sizeLabels[i])
                            .font(.caption.weight(.semibold))
                            .padding(.horizontal, 9)
                            .padding(.vertical, 5)
                            .background(RoundedRectangle(cornerRadius: 8)
                                .fill(btext.textSizeIdx == i ? AppTheme.accent : AppTheme.sunken))
                            .foregroundColor(btext.textSizeIdx == i ? .white : AppTheme.text)
                    }
                    .buttonStyle(.plain)
                }
            }
            HStack(spacing: 8) {
                Text("颜色")
                    .font(.footnote)
                    .foregroundColor(AppTheme.sub)
                ForEach(BadgeTextBridge.textColors.indices, id: \.self) { i in
                    Button {
                        btext.textColorIdx = i
                    } label: {
                        Circle()
                            .fill(Color(BadgeTextBridge.textColors[i]))
                            .frame(width: 24, height: 24)
                            .overlay(Circle().stroke(
                                btext.textColorIdx == i ? AppTheme.accent : Color.clear,
                                lineWidth: 2))
                    }
                    .buttonStyle(.plain)
                }
            }
            Button { btext.uploadTextToDevice() } label: {
                Text("上传文字")
                    .font(.subheadline.weight(.semibold))
                    .foregroundColor(.white)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 11)
                    .background(RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(!btext.badgeText.isEmpty && ble.isReady ? AppTheme.accent : AppTheme.accent.opacity(0.45)))
            }
            .buttonStyle(.plain)
            .disabled(btext.badgeText.trimmingCharacters(in: .whitespaces).isEmpty || !ble.isReady)
            if !btext.textStatus.isEmpty {
                Text(btext.textStatus).font(.caption).foregroundColor(AppTheme.sub)
            }
            if !ble.isReady {
                Text("连接圆屏后上传；吧唧页（主屏上划）即横向滚动。")
                    .font(.caption2)
                    .foregroundColor(AppTheme.sub)
            }
        }
    }

    private var badgeImageSection: some View {
        VStack(alignment: .leading, spacing: 8) {
                    if !badge.hasImage {
                        Text("圆屏静图展示")
                            .font(.subheadline.weight(.semibold))
                            .foregroundColor(AppTheme.text)
                        Text("先选择图片，再上传到设备。主屏上划查看。")
                            .font(.footnote)
                            .foregroundColor(AppTheme.sub)
                            .fixedSize(horizontal: false, vertical: true)
                    } else {
                        Text(badge.pushed ? "已上传 · 圆裁预览" : "已选择 · 圆裁预览")
                            .font(.subheadline.weight(.semibold))
                            .foregroundColor(AppTheme.text)
                        Text("与设备电子吧唧页一致。未导航时主屏上划查看。")
                            .font(.footnote)
                            .foregroundColor(AppTheme.sub)
                            .fixedSize(horizontal: false, vertical: true)
                        if badge.pushed {
                            Label("已推送到圆屏", systemImage: "checkmark.circle.fill")
                                .font(.caption.weight(.semibold))
                                .foregroundColor(AppTheme.green)
                        } else if !badge.status.isEmpty {
                            Text(badge.status)
                                .font(.caption)
                                .foregroundColor(AppTheme.sub)
                        }
                    }

                    HStack(spacing: 8) {
                        Button { showPicker = true } label: {
                            Text(badge.hasImage ? "更换图片" : "选择图片")
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(AppTheme.text)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 11)
                                .background(
                                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                                        .fill(AppTheme.sunken)
                                )
                        }
                        .buttonStyle(.plain)
                        .disabled(badge.transferring)

                        Button { badge.uploadToDevice() } label: {
                            Text("上传图片")
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 11)
                                .background(
                                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                                        .fill(badge.hasImage && ble.isReady ? AppTheme.accent : AppTheme.accent.opacity(0.45))
                                )
                        }
                        .buttonStyle(.plain)
                        .disabled(!badge.hasImage || badge.transferring || !ble.isReady)
                    }

                    HStack(spacing: 8) {
                        Button { showCrop = true } label: {
                            Text("裁切")
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(AppTheme.text)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 10)
                                .background(
                                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                                        .fill(AppTheme.sunken)
                                )
                        }
                        .buttonStyle(.plain)
                        .disabled(!badge.hasImage || badge.transferring)

                        if badge.hasImage {
                            Button { badge.clearImage() } label: {
                                Text("清除")
                                    .font(.subheadline.weight(.semibold))
                                    .foregroundColor(AppTheme.sub)
                                    .frame(maxWidth: .infinity)
                                    .padding(.vertical, 10)
                                    .background(
                                        RoundedRectangle(cornerRadius: 12, style: .continuous)
                                            .fill(AppTheme.sunken.opacity(0.6))
                                    )
                            }
                            .buttonStyle(.plain)
                            .disabled(badge.transferring)
                        }
                    }

                    if !ble.isReady {
                        Text("可先选图预览；连接圆屏后再点「上传图片」")
                            .font(.caption2)
                            .foregroundColor(AppTheme.sub)
                    }
        }
    }

    private var badgePreviewRing: some View {
        ZStack {
            Ellipse()
                .fill(Color.black.opacity(0.18))
                .frame(width: 78, height: 14)
                .blur(radius: 3)
                .offset(y: 48)
            Circle()
                .stroke(Color.dynamic(
                    light: UIColor(white: 0.72, alpha: 1),
                    dark: UIColor(white: 0.55, alpha: 1)), lineWidth: 3)
                .frame(width: 92, height: 92)
            if let img = badge.previewImage {
                Image(uiImage: img)
                    .resizable()
                    .scaledToFill()
                    .frame(width: 84, height: 84)
                    .clipShape(Circle())
            } else {
                Circle()
                    .fill(AppTheme.sunken)
                    .frame(width: 84, height: 84)
                    .overlay(
                        Image(systemName: "plus")
                            .font(.system(size: 22, weight: .medium))
                            .foregroundColor(AppTheme.sub)
                    )
            }
            if badge.transferring {
                ProgressView()
                    .progressViewStyle(.circular)
            }
        }
        .frame(width: 96, height: 104)
        .padding(.top, 2)
    }
}

/// iOS 15–compatible album picker for 电子吧唧.
private struct BadgeImagePicker: UIViewControllerRepresentable {
    var onPick: (UIImage?) -> Void

    func makeCoordinator() -> Coordinator { Coordinator(onPick: onPick) }

    func makeUIViewController(context: Context) -> UIImagePickerController {
        let p = UIImagePickerController()
        p.sourceType = .photoLibrary
        p.allowsEditing = false
        p.delegate = context.coordinator
        return p
    }

    func updateUIViewController(_ uiViewController: UIImagePickerController, context: Context) {}

    final class Coordinator: NSObject, UIImagePickerControllerDelegate, UINavigationControllerDelegate {
        let onPick: (UIImage?) -> Void
        init(onPick: @escaping (UIImage?) -> Void) { self.onPick = onPick }
        func imagePickerControllerDidCancel(_ picker: UIImagePickerController) { onPick(nil) }
        func imagePickerController(_ picker: UIImagePickerController,
                                   didFinishPickingMediaWithInfo info: [UIImagePickerController.InfoKey: Any]) {
            onPick(info[.originalImage] as? UIImage)
        }
    }
}

/// 自定义开机背景卡（musicA.17）：选图 → 推送 bulk type 8；持久化 = 开机连 App 自动重推。
struct DeviceBackgroundCard: View {
    @ObservedObject var bg = BackgroundBridge.shared
    @ObservedObject var ble = BleLink.shared
    @State private var showPicker = false
    @State private var showCrop = false

    var body: some View {
        CardView {
            HStack(alignment: .top, spacing: 14) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("自定义开机背景")
                        .font(.system(size: 20, weight: .bold))
                        .foregroundColor(AppTheme.text)
                    Text(bg.hasImage
                         ? (bg.pushed ? "已推送到圆屏 · 每次开机连 App 自动重推" : "已选择 · 连接圆屏后自动重推")
                         : "把相册图片设为待机页背景；圆形预览与圆屏一致。")
                        .font(.footnote)
                        .foregroundColor(AppTheme.sub)
                        .fixedSize(horizontal: false, vertical: true)
                    if !bg.status.isEmpty {
                        Text(bg.status).font(.caption).foregroundColor(AppTheme.sub)
                    }
                    HStack(spacing: 8) {
                        Button { showPicker = true } label: {
                            Text(bg.hasImage ? "更换图片" : "选择图片")
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(AppTheme.text)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 11)
                                .background(RoundedRectangle(cornerRadius: 12, style: .continuous).fill(AppTheme.sunken))
                        }
                        .buttonStyle(.plain)
                        .disabled(bg.transferring)

                        Button { bg.pushToDevice() } label: {
                            Text("推送到圆屏")
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 11)
                                .background(RoundedRectangle(cornerRadius: 12, style: .continuous)
                                    .fill(bg.hasImage && ble.isReady ? AppTheme.accent : AppTheme.accent.opacity(0.45)))
                        }
                        .buttonStyle(.plain)
                        .disabled(!bg.hasImage || bg.transferring || !ble.isReady)
                    }
                    HStack(spacing: 8) {
                        Button { showCrop = true } label: {
                            Text("裁切")
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(AppTheme.text)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 10)
                                .background(RoundedRectangle(cornerRadius: 12, style: .continuous).fill(AppTheme.sunken))
                        }
                        .buttonStyle(.plain)
                        .disabled(!bg.hasImage || bg.transferring)

                        if bg.hasImage {
                            Button { bg.clearImage() } label: {
                                Text("恢复默认")
                                    .font(.subheadline.weight(.semibold))
                                    .foregroundColor(AppTheme.sub)
                                    .frame(maxWidth: .infinity)
                                    .padding(.vertical, 10)
                                    .background(RoundedRectangle(cornerRadius: 12, style: .continuous).fill(AppTheme.sunken.opacity(0.6)))
                            }
                            .buttonStyle(.plain)
                            .disabled(bg.transferring)
                        }
                    }
                    Text("持久化方式：开机连 App 自动重推（掉电不丢）。App 本地保留最后一次选的图。")
                        .font(.caption2).foregroundColor(AppTheme.sub)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                backgroundPreview
            }
        }
        .sheet(isPresented: $showPicker) {
            BadgeImagePicker { img in
                showPicker = false
                if let img { bg.selectImage(img) }
            }
            .ignoresSafeArea()
        }
        .sheet(isPresented: $showCrop) {
            if let source = bg.sourceImage {
                BadgeCropEditor(
                    source: source,
                    initialMode: bg.cropMode,
                    initialCrop: bg.normalizedCrop
                        ?? (bg.cropMode == .manualRegion
                            ? BadgeBridge.defaultFreeCrop(for: source)
                            : BadgeBridge.defaultNormalizedCrop(for: source)),
                    onCancel: { showCrop = false },
                    onDone: { mode, norm in
                        showCrop = false
                        bg.applyCrop(mode: mode, normalizedCrop: norm)
                    }
                )
            } else {
                Text("无可裁切图片").padding()
            }
        }
    }

    private var backgroundPreview: some View {
        ZStack {
            Ellipse()
                .fill(Color.black.opacity(0.18))
                .frame(width: 78, height: 14)
                .blur(radius: 3)
                .offset(y: 48)
            Circle()
                .stroke(Color.dynamic(
                    light: UIColor(white: 0.72, alpha: 1),
                    dark: UIColor(white: 0.55, alpha: 1)), lineWidth: 3)
                .frame(width: 92, height: 92)
            if let img = bg.previewImage {
                Image(uiImage: img)
                    .resizable()
                    .scaledToFill()
                    .frame(width: 84, height: 84)
                    .clipShape(Circle())
            } else {
                Circle()
                    .fill(AppTheme.sunken)
                    .frame(width: 84, height: 84)
                    .overlay(
                        Image(systemName: "photo")
                            .font(.system(size: 22, weight: .medium))
                            .foregroundColor(AppTheme.sub)
                    )
            }
            if bg.transferring {
                ProgressView().progressViewStyle(.circular)
            }
        }
        .frame(width: 96, height: 104)
        .padding(.top, 2)
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
                    GlassChip(title: "重新配对", icon: "arrow.triangle.2.circlepath", tint: AppTheme.accent, compact: true) { tpms.startRepair() }
                    GlassChip(title: "发送测试帧", foreground: AppTheme.sub, compact: true) { tpms.sendTest() }
                }
                Text("扫描中 · 最近心跳 \(tpms.lastAdvs) 条广播 · 已配对 \(tpms.pairedCount)/2")
                    .font(.caption2).foregroundColor(AppTheme.sub)
            }
        }
        .sheet(isPresented: $tpms.repairing) {
            TpmsRepairSheet()
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

/// 重新配对胎压气嘴：弹窗列出扫描到的气嘴，用户手动指定前轮/后轮，避免前后轮分不清。
struct TpmsRepairSheet: View {
    @ObservedObject private var tpms = TpmsManager.shared
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationView {
            List {
                Section {
                    if tpms.candidates.isEmpty {
                        Text("正在扫描气嘴…\n请触发气嘴广播（上电瞬间 / 按下气嘴 / 转动车轮），扫到的气嘴会列在下面。")
                            .font(.caption).foregroundColor(AppTheme.sub)
                    } else {
                        ForEach(tpms.candidates) { c in
                            VStack(alignment: .leading, spacing: 6) {
                                HStack {
                                    Text("气嘴 \(c.id.prefix(8))…").font(.subheadline).foregroundColor(AppTheme.text)
                                    Spacer()
                                    Text("\(c.rssi) dBm").font(.caption2).foregroundColor(AppTheme.sub)
                                }
                                Text("\(String(format: "%.1f", Double(c.reading.kpa) / 100.0)) bar · \(c.reading.tempC)°C · 电量 \(c.reading.battPct)%")
                                    .font(.caption).foregroundColor(AppTheme.sub)
                                HStack(spacing: 10) {
                                    Button { tpms.assignFront(c.id) } label: {
                                        Text("设为前轮").frame(maxWidth: .infinity)
                                    }
                                    .buttonStyle(.borderedProminent)
                                    .tint(tpms.frontId == c.id ? AppTheme.green : AppTheme.accent)
                                    Button { tpms.assignRear(c.id) } label: {
                                        Text("设为后轮").frame(maxWidth: .infinity)
                                    }
                                    .buttonStyle(.bordered)
                                    .tint(tpms.rearId == c.id ? AppTheme.green : AppTheme.accent)
                                }
                            }
                            .padding(.vertical, 4)
                        }
                    }
                } header: { Text("扫描到的气嘴（点按钮指定前后轮）") }

                Section {
                    HStack {
                        Text("前轮").foregroundColor(AppTheme.text)
                        Spacer()
                        Text(tpms.frontId.isEmpty ? "未指定" : "\(tpms.frontId.prefix(8))… 已指定")
                            .foregroundColor(tpms.frontId.isEmpty ? AppTheme.sub : AppTheme.green)
                    }
                    HStack {
                        Text("后轮").foregroundColor(AppTheme.text)
                        Spacer()
                        Text(tpms.rearId.isEmpty ? "未指定" : "\(tpms.rearId.prefix(8))… 已指定")
                            .foregroundColor(tpms.rearId.isEmpty ? AppTheme.sub : AppTheme.green)
                    }
                } header: { Text("当前指定") }
            }
            .navigationTitle("重新配对胎压气嘴")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("取消") { tpms.cancelRepair() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("完成") { tpms.cancelRepair() }
                        .disabled(tpms.frontId.isEmpty || tpms.rearId.isEmpty)
                }
            }
        }
        .navigationViewStyle(.stack)
    }
}
