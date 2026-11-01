import SwiftUI
import UIKit

/// 电子吧唧裁切编辑器：分段「自动居中 | 自选区域」；输出仍 466×466。
/// iOS 15：不用 PhotosPicker / iOS 17 onChange。
struct BadgeCropEditor: View {
    let source: UIImage
    var initialMode: BadgeCropMode
    var initialCrop: CGRect?
    var onCancel: () -> Void
    var onDone: (_ mode: BadgeCropMode, _ normalized: CGRect?) -> Void

    @State private var mode: BadgeCropMode
    /// 归一化方选区（相对原图）
    @State private var crop: CGRect
    @State private var dragStartCrop: CGRect?

    init(source: UIImage,
         initialMode: BadgeCropMode = .autoCenter,
         initialCrop: CGRect? = nil,
         onCancel: @escaping () -> Void,
         onDone: @escaping (_ mode: BadgeCropMode, _ normalized: CGRect?) -> Void) {
        self.source = source
        self.initialMode = initialMode
        self.initialCrop = initialCrop
        self.onCancel = onCancel
        self.onDone = onDone
        _mode = State(initialValue: initialMode)
        let def = initialCrop ?? BadgeBridge.defaultNormalizedCrop(for: source)
        _crop = State(initialValue: def)
    }

    var body: some View {
        NavigationView {
            VStack(spacing: 16) {
                Picker("", selection: $mode) {
                    ForEach(BadgeCropMode.allCases, id: \.rawValue) { m in
                        Text(m.title).tag(m)
                    }
                }
                .pickerStyle(.segmented)
                .padding(.horizontal, 16)

                Text(mode == .autoCenter
                     ? "短边居中方裁 · 输出 466×466 · BLE 类型 7"
                     : "拖动选区 · 仍输出 466×466")
                    .font(.caption)
                    .foregroundColor(AppTheme.sub)

                GeometryReader { geo in
                    let fit = fittedRect(in: geo.size)
                    ZStack {
                        Color.black.opacity(0.92).ignoresSafeArea()
                        Image(uiImage: source)
                            .resizable()
                            .scaledToFit()
                            .frame(width: fit.width, height: fit.height)
                            .position(x: geo.size.width / 2, y: geo.size.height / 2)

                        // 暗角遮罩 + 方选框
                        CropMask(imageFrame: fit, normalizedCrop: displayCrop, canvas: geo.size)
                            .allowsHitTesting(false)

                        if mode == .manualRegion {
                            CropHandle(imageFrame: fit, crop: $crop, canvas: geo.size)
                        }
                    }
                }
                .frame(maxHeight: .infinity)

                // 圆预览（与推送裁切一致）
                if let preview = BadgeBridge.cropSquare(source, mode: mode,
                                                       normalized: mode == .manualRegion ? crop : nil) {
                    VStack(spacing: 8) {
                        Image(uiImage: preview)
                            .resizable()
                            .scaledToFill()
                            .frame(width: 96, height: 96)
                            .clipShape(Circle())
                            .overlay(Circle().strokeBorder(AppTheme.stroke.opacity(0.5), lineWidth: 2))
                        Text("圆裁预览")
                            .font(.caption2)
                            .foregroundColor(AppTheme.sub)
                    }
                    .padding(.bottom, 8)
                }
            }
            .background(AppTheme.bg.ignoresSafeArea())
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("取消", action: onCancel)
                }
                ToolbarItem(placement: .principal) {
                    Text("裁切").font(.headline)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("完成") {
                        onDone(mode, mode == .manualRegion ? crop : nil)
                    }
                    .fontWeight(.semibold)
                }
            }
        }
        .navigationViewStyle(.stack)
    }

    private var displayCrop: CGRect {
        mode == .autoCenter ? BadgeBridge.defaultNormalizedCrop(for: source) : crop
    }

    private func fittedRect(in size: CGSize) -> CGRect {
        let iw = source.size.width, ih = source.size.height
        guard iw > 0, ih > 0 else { return .zero }
        let scale = min(size.width / iw, size.height / ih)
        let w = iw * scale, h = ih * scale
        return CGRect(x: (size.width - w) / 2, y: (size.height - h) / 2, width: w, height: h)
    }
}

private struct CropMask: View {
    let imageFrame: CGRect
    let normalizedCrop: CGRect
    let canvas: CGSize

    var body: some View {
        let r = absRect
        Canvas { ctx, size in
            var path = Path(CGRect(origin: .zero, size: size))
            path.addRect(r)
            ctx.fill(path, with: .color(Color.black.opacity(0.55)), style: FillStyle(eoFill: true))
            ctx.stroke(Path(roundedRect: r, cornerRadius: 2), with: .color(Color.white.opacity(0.95)), lineWidth: 2)
        }
        .frame(width: canvas.width, height: canvas.height)
    }

    private var absRect: CGRect {
        CGRect(x: imageFrame.minX + normalizedCrop.minX * imageFrame.width,
               y: imageFrame.minY + normalizedCrop.minY * imageFrame.height,
               width: normalizedCrop.width * imageFrame.width,
               height: normalizedCrop.height * imageFrame.height)
    }
}

/// 拖动方选区（保持正方形，夹在图内）。
private struct CropHandle: View {
    let imageFrame: CGRect
    @Binding var crop: CGRect
    let canvas: CGSize
    @State private var start: CGRect?

    var body: some View {
        let r = absRect
        RoundedRectangle(cornerRadius: 2, style: .continuous)
            .strokeBorder(Color.white, lineWidth: 2)
            .background(Color.white.opacity(0.04))
            .frame(width: r.width, height: r.height)
            .position(x: r.midX, y: r.midY)
            .gesture(
                DragGesture()
                    .onChanged { val in
                        if start == nil { start = crop }
                        guard let s = start, imageFrame.width > 0, imageFrame.height > 0 else { return }
                        let dx = val.translation.width / imageFrame.width
                        let dy = val.translation.height / imageFrame.height
                        let side = min(s.width, s.height)
                        var nx = s.origin.x + dx
                        var ny = s.origin.y + dy
                        nx = max(0, min(nx, 1 - side))
                        ny = max(0, min(ny, 1 - side))
                        crop = CGRect(x: nx, y: ny, width: side, height: side)
                    }
                    .onEnded { _ in start = nil }
            )
            // 四角缩放：右下角
            .overlay(alignment: .bottomTrailing) {
                Circle()
                    .fill(Color.white)
                    .frame(width: 22, height: 22)
                    .shadow(color: .black.opacity(0.25), radius: 2, y: 1)
                    .offset(x: 8, y: 8)
                    .gesture(
                        DragGesture()
                            .onChanged { val in
                                if start == nil { start = crop }
                                guard let s = start, imageFrame.width > 0 else { return }
                                let delta = max(val.translation.width / imageFrame.width,
                                                val.translation.height / imageFrame.height)
                                var side = min(s.width, s.height) + delta
                                side = max(0.18, min(side, min(1 - s.origin.x, 1 - s.origin.y)))
                                crop = CGRect(x: s.origin.x, y: s.origin.y, width: side, height: side)
                            }
                            .onEnded { _ in start = nil }
                    )
            }
            .frame(width: canvas.width, height: canvas.height)
    }

    private var absRect: CGRect {
        CGRect(x: imageFrame.minX + crop.minX * imageFrame.width,
               y: imageFrame.minY + crop.minY * imageFrame.height,
               width: crop.width * imageFrame.width,
               height: crop.height * imageFrame.height)
    }
}
