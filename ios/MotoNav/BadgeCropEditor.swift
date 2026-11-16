import SwiftUI
import UIKit

/// 电子吧唧裁切编辑器：分段「自动居中 | 自选区域」。
/// 自选区域支持**自由宽高比**；导出时在选区内居中裁方再缩到 466×466。
/// iOS 15：不用 PhotosPicker / iOS 17 onChange。
struct BadgeCropEditor: View {
    let source: UIImage
    var initialMode: BadgeCropMode
    var initialCrop: CGRect?
    var onCancel: () -> Void
    var onDone: (_ mode: BadgeCropMode, _ normalized: CGRect?) -> Void

    @State private var mode: BadgeCropMode
    /// 归一化选区（相对原图）；manual 可为非正方形。
    @State private var crop: CGRect

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
                     : "自由拖拽选区（可调宽高比）· 导出时居中裁方 → 466×466")
                    .font(.caption)
                    .foregroundColor(AppTheme.sub)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 16)

                GeometryReader { geo in
                    let fit = fittedRect(in: geo.size)
                    ZStack {
                        Color.black.opacity(0.92).ignoresSafeArea()
                        Image(uiImage: source)
                            .resizable()
                            .scaledToFit()
                            .frame(width: fit.width, height: fit.height)
                            .position(x: geo.size.width / 2, y: geo.size.height / 2)

                        CropMask(imageFrame: fit, normalizedCrop: displayCrop, canvas: geo.size)
                            .allowsHitTesting(false)

                        if mode == .manualRegion {
                            FreeCropHandle(imageFrame: fit, crop: $crop, canvas: geo.size)
                        }
                    }
                }
                .frame(maxHeight: .infinity)

                if let preview = BadgeBridge.cropToDeviceSquare(source, mode: mode,
                                                               normalized: mode == .manualRegion ? crop : nil) {
                    VStack(spacing: 8) {
                        Image(uiImage: preview)
                            .resizable()
                            .scaledToFill()
                            .frame(width: 96, height: 96)
                            .clipShape(Circle())
                            .overlay(Circle().stroke(AppTheme.stroke.opacity(0.5), lineWidth: 2))
                        Text("设备预览 · 466 方裁")
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
                    Button(action: {
                        onDone(mode, mode == .manualRegion ? crop : nil)
                    }) {
                        Text("完成").font(.body.weight(.semibold))
                    }
                }
            }
        }
        .navigationViewStyle(.stack)
        .onAppear {
            if mode == .manualRegion, crop.width == crop.height,
               abs(crop.width - 1) > 0.01 {
                // keep prior square crops; if none meaningful, start near full frame
            }
        }
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

/// 自由宽高比选区：拖动平移；四角独立缩放宽高。
private struct FreeCropHandle: View {
    let imageFrame: CGRect
    @Binding var crop: CGRect
    let canvas: CGSize
    @State private var start: CGRect?

    var body: some View {
        let r = absRect
        ZStack {
            RoundedRectangle(cornerRadius: 2, style: .continuous)
                .fill(Color.white.opacity(0.04))
                .frame(width: r.width, height: r.height)
                .position(x: r.midX, y: r.midY)
                .gesture(moveGesture)

            RoundedRectangle(cornerRadius: 2, style: .continuous)
                .stroke(Color.white, lineWidth: 2)
                .frame(width: r.width, height: r.height)
                .position(x: r.midX, y: r.midY)
                .allowsHitTesting(false)

            cornerHandle(at: CGPoint(x: r.minX, y: r.minY), corner: .topLeading)
            cornerHandle(at: CGPoint(x: r.maxX, y: r.minY), corner: .topTrailing)
            cornerHandle(at: CGPoint(x: r.minX, y: r.maxY), corner: .bottomLeading)
            cornerHandle(at: CGPoint(x: r.maxX, y: r.maxY), corner: .bottomTrailing)
        }
        .frame(width: canvas.width, height: canvas.height)
    }

    private enum Corner { case topLeading, topTrailing, bottomLeading, bottomTrailing }

    private func cornerHandle(at point: CGPoint, corner: Corner) -> some View {
        Circle()
            .fill(Color.white)
            .frame(width: 22, height: 22)
            .shadow(color: .black.opacity(0.25), radius: 2, y: 1)
            .position(point)
            .gesture(
                DragGesture()
                    .onChanged { val in
                        if start == nil { start = crop }
                        guard let s = start, imageFrame.width > 0, imageFrame.height > 0 else { return }
                        let dx = val.translation.width / imageFrame.width
                        let dy = val.translation.height / imageFrame.height
                        var x = s.origin.x, y = s.origin.y, w = s.width, h = s.height
                        switch corner {
                        case .topLeading:
                            x = s.origin.x + dx; y = s.origin.y + dy
                            w = s.width - dx; h = s.height - dy
                        case .topTrailing:
                            y = s.origin.y + dy
                            w = s.width + dx; h = s.height - dy
                        case .bottomLeading:
                            x = s.origin.x + dx
                            w = s.width - dx; h = s.height + dy
                        case .bottomTrailing:
                            w = s.width + dx; h = s.height + dy
                        }
                        crop = clamp(CGRect(x: x, y: y, width: w, height: h))
                    }
                    .onEnded { _ in start = nil }
            )
    }

    private var moveGesture: some Gesture {
        DragGesture()
            .onChanged { val in
                if start == nil { start = crop }
                guard let s = start, imageFrame.width > 0, imageFrame.height > 0 else { return }
                let dx = val.translation.width / imageFrame.width
                let dy = val.translation.height / imageFrame.height
                crop = clamp(CGRect(x: s.origin.x + dx, y: s.origin.y + dy, width: s.width, height: s.height))
            }
            .onEnded { _ in start = nil }
    }

    private func clamp(_ r: CGRect) -> CGRect {
        var x = r.origin.x, y = r.origin.y, w = r.width, h = r.height
        let minSide: CGFloat = 0.12
        w = max(minSide, w); h = max(minSide, h)
        if x < 0 { w += x; x = 0 }
        if y < 0 { h += y; y = 0 }
        if x + w > 1 { w = 1 - x }
        if y + h > 1 { h = 1 - y }
        w = max(minSide, min(w, 1)); h = max(minSide, min(h, 1))
        x = max(0, min(x, 1 - w)); y = max(0, min(y, 1 - h))
        return CGRect(x: x, y: y, width: w, height: h)
    }

    private var absRect: CGRect {
        CGRect(x: imageFrame.minX + crop.minX * imageFrame.width,
               y: imageFrame.minY + crop.minY * imageFrame.height,
               width: crop.width * imageFrame.width,
               height: crop.height * imageFrame.height)
    }
}
