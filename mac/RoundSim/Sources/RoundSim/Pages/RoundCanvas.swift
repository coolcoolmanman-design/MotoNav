import SwiftUI

/// Exact 466×466 circular viewport (Waveshare 1.75). Retina scale via SwiftUI.
struct RoundCanvas: View {
    @ObservedObject var state: SimState

    var body: some View {
        ZStack {
            if state.showBezel {
                Circle()
                    .fill(
                        RadialGradient(
                            colors: [Color(white: 0.18), Color(white: 0.05)],
                            center: .center, startRadius: 200, endRadius: 280)
                    )
                    .frame(width: 520, height: 520)
                    .shadow(color: .black.opacity(0.55), radius: 24, y: 10)
            }

            ZStack {
                pageContent
            }
            .frame(width: SimState.lcdSize, height: SimState.lcdSize)
            .clipped()
            .clipShape(Circle())
            .overlay(Circle().stroke(Color.white.opacity(0.08), lineWidth: 1))
            .contentShape(Circle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onEnded { value in
                        state.handleCanvasTap(value.location)
                    }
            )
        }
        .frame(width: 560, height: 560)
    }

    @ViewBuilder
    private var pageContent: some View {
        switch state.page {
        case .idle: IdlePage(state: state)
        case .music: MusicPage(state: state)
        case .nav: NavHudPage(state: state)
        case .arrival: ArrivalPage(state: state)
        }
    }
}

struct IdlePage: View {
    @ObservedObject var state: SimState
    private var time: String {
        let f = DateFormatter(); f.dateFormat = "HH:mm"
        return f.string(from: state.clockTick)
    }
    private var date: String {
        let f = DateFormatter(); f.dateFormat = "EEE M/d"
        return f.string(from: state.clockTick)
    }

    var body: some View {
        ZStack {
            Color.black
            // Tick ring
            Canvas { ctx, size in
                let c = CGPoint(x: size.width / 2, y: size.height / 2)
                for i in 0..<60 {
                    let a = Double(i) / 60 * 2 * Double.pi - Double.pi / 2
                    let major = i % 5 == 0
                    let r0: CGFloat = major ? 200 : 210
                    let r1: CGFloat = 220
                    var path = Path()
                    path.move(to: CGPoint(x: c.x + cos(a) * r0, y: c.y + sin(a) * r0))
                    path.addLine(to: CGPoint(x: c.x + cos(a) * r1, y: c.y + sin(a) * r1))
                    ctx.stroke(path, with: .color(.white.opacity(major ? 0.55 : 0.2)), lineWidth: major ? 2 : 1)
                }
            }
            VStack(spacing: 8) {
                Text("MotoNav")
                    .font(.system(size: 28, weight: .semibold, design: .rounded))
                    .foregroundStyle(Color.cyan)
                Text(time)
                    .font(.system(size: 72, weight: .thin, design: .rounded))
                    .foregroundStyle(.white)
                    .monospacedDigit()
                Text(date)
                    .font(.system(size: 16, weight: .medium, design: .rounded))
                    .foregroundStyle(.white.opacity(0.55))
                Text(state.connectedClients > 0 ? "ready · RoundSim" : "no link")
                    .font(.system(size: 14, design: .rounded))
                    .foregroundStyle(state.connectedClients > 0 ? Color.green : Color.gray)
                Text("fw sim · 466×466")
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundStyle(.white.opacity(0.35))
            }
        }
    }
}

struct MusicPage: View {
    @ObservedObject var state: SimState

    var body: some View {
        ZStack {
            Color.black
            // Ambient wash from cover (stub)
            if state.coverImage != nil {
                Circle()
                    .fill(Color.cyan.opacity(0.12))
                    .frame(width: 420, height: 420)
                    .blur(radius: 40)
            }
            VStack(spacing: 10) {
                ZStack {
                    // Vinyl disc + cover
                    Circle()
                        .fill(Color(white: 0.08))
                        .frame(width: 220, height: 220)
                        .overlay(
                            Group {
                                if let cover = state.coverImage {
                                    Image(nsImage: cover)
                                        .resizable()
                                        .scaledToFill()
                                        .frame(width: 200, height: 200)
                                        .clipShape(Circle())
                                        .rotationEffect(.radians(state.coverAngle))
                                } else {
                                    Text("MUSIC")
                                        .font(.system(size: 22, weight: .bold, design: .rounded))
                                        .foregroundStyle(Color.cyan)
                                }
                            }
                        )
                        .overlay(Circle().stroke(Color.white.opacity(0.25), lineWidth: 1))

                    // Tonearm (play lowers onto disc, pause raises)
                    TonearmView(progress: state.tonearmT)
                        .frame(width: 240, height: 240)
                }
                .padding(.top, 24)

                Text(state.titleSlot.isEmpty ? (state.musicHave ? "TRACK" : "NO PLAYER") : state.titleSlot)
                    .font(.system(size: 15, weight: .semibold, design: .rounded))
                    .foregroundStyle(.white)
                Text(state.artistSlot.isEmpty ? (state.musicPlaying ? "PLAYING" : "PAUSED") : state.artistSlot)
                    .font(.system(size: 12, design: .rounded))
                    .foregroundStyle(.white.opacity(0.55))

                HStack(spacing: 36) {
                    ForEach(0..<3, id: \.self) { i in
                        Circle()
                            .fill(Color(white: 0.14))
                            .frame(width: 56, height: 56)
                            .overlay(musicGlyph(i).foregroundStyle(.white))
                    }
                }
                .padding(.top, 8)
                Spacer(minLength: 20)
            }
        }
    }

    @ViewBuilder
    private func musicGlyph(_ i: Int) -> some View {
        switch i {
        case 0: Image(systemName: "backward.fill").font(.system(size: 16))
        case 1: Image(systemName: state.musicPlaying ? "pause.fill" : "play.fill").font(.system(size: 18))
        default: Image(systemName: "forward.fill").font(.system(size: 16))
        }
    }
}

struct TonearmView: View {
    var progress: Double // 0 raised → 1 on disc
    var body: some View {
        GeometryReader { geo in
            let pivot = CGPoint(x: geo.size.width * 0.82, y: geo.size.height * 0.18)
            let angle = Angle.degrees(-35 + progress * 28)
            Path { p in
                p.move(to: pivot)
                p.addLine(to: CGPoint(x: pivot.x - 95, y: pivot.y + 70))
            }
            .stroke(Color.white.opacity(0.85), style: StrokeStyle(lineWidth: 3, lineCap: .round))
            .rotationEffect(angle, anchor: UnitPoint(x: pivot.x / geo.size.width, y: pivot.y / geo.size.height))
            Circle()
                .fill(Color.white.opacity(0.7))
                .frame(width: 10, height: 10)
                .position(pivot)
            // black stylus tip
            Circle()
                .fill(Color.black)
                .frame(width: 8, height: 8)
                .position(x: pivot.x - 95, y: pivot.y + 70)
                .rotationEffect(angle, anchor: UnitPoint(x: pivot.x / geo.size.width, y: pivot.y / geo.size.height))
        }
        .allowsHitTesting(false)
    }
}

struct NavHudPage: View {
    @ObservedObject var state: SimState

    var body: some View {
        ZStack {
            Color.black
            // Map placeholder disc
            Circle()
                .fill(
                    RadialGradient(
                        colors: [
                            state.nightOn ? Color(red: 0.05, green: 0.08, blue: 0.12) : Color(red: 0.08, green: 0.14, blue: 0.1),
                            Color.black
                        ],
                        center: .center, startRadius: 20, endRadius: 220)
                )
            VStack(spacing: 6) {
                Text(state.rerouting ? "REROUTING…" : (state.gpsWeak ? "GPS WEAK" : "NAV"))
                    .font(.system(size: 13, weight: .bold, design: .rounded))
                    .foregroundStyle(state.gpsWeak ? Color.orange : Color.cyan)
                Text(state.navDir.isEmpty ? "—" : state.navDir)
                    .font(.system(size: 48, weight: .bold, design: .rounded))
                    .foregroundStyle(.white)
                Text("\(state.navDist) m")
                    .font(.system(size: 28, weight: .medium, design: .rounded))
                    .foregroundStyle(.white.opacity(0.85))
                Text("\(state.speedKmh) km/h")
                    .font(.system(size: 22, design: .rounded))
                    .foregroundStyle(Color.green)
                if state.remainM > 0 {
                    Text("remain \(state.remainM) m · ETA \(state.etaHHMM)")
                        .font(.system(size: 12, design: .monospaced))
                        .foregroundStyle(.white.opacity(0.5))
                }
                Text(state.mapStatus)
                    .font(.system(size: 10, design: .monospaced))
                    .foregroundStyle(.white.opacity(0.35))
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 40)
                Text(state.mapOn ? "map ON · stub" : "map OFF")
                    .font(.system(size: 11))
                    .foregroundStyle(.white.opacity(0.4))
            }
        }
    }
}

struct ArrivalPage: View {
    @ObservedObject var state: SimState
    private var pulse: Double {
        let t = Date().timeIntervalSince(state.arrivalStartedAt)
        return 0.55 + 0.45 * abs(sin(t * 2.2))
    }

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30.0)) { timeline in
            let t = timeline.date.timeIntervalSince(state.arrivalStartedAt)
            ZStack {
                Color.black
                // Rays
                ForEach(0..<16, id: \.self) { i in
                    let rot = t * 0.25 + Double(i) * (.pi * 2 / 16)
                    Path { p in
                        p.move(to: CGPoint(x: 233, y: 233))
                        p.addLine(to: CGPoint(x: 233 + cos(rot) * 260, y: 233 + sin(rot) * 260))
                        p.addLine(to: CGPoint(x: 233 + cos(rot + 0.11) * 260, y: 233 + sin(rot + 0.11) * 260))
                        p.closeSubpath()
                    }
                    .fill((state.arrivalWaypoint ? Color.cyan : Color.pink).opacity(0.18))
                }

                VStack(spacing: 10) {
                    Text(state.arrivalWaypoint ? "WAYPOINT" : "ARRIVED")
                        .font(.system(size: 36, weight: .heavy, design: .rounded))
                        .foregroundStyle(.white)
                        .shadow(color: (state.arrivalWaypoint ? Color.cyan : Color.pink).opacity(0.8), radius: 8)
                    if state.tripM >= 0 {
                        Text(tripLine)
                            .font(.system(size: 16, design: .rounded))
                            .foregroundStyle(Color.cyan)
                    } else {
                        Text("RIDE SAFE")
                            .font(.system(size: 16, design: .rounded))
                            .foregroundStyle(Color.cyan)
                    }
                    if state.tripMaxKmh > 0 {
                        Text("MAX \(state.tripMaxKmh) km/h")
                            .font(.system(size: 13, design: .rounded))
                            .foregroundStyle(Color.yellow.opacity(0.85))
                    }
                    Spacer().frame(height: 48)
                }

                // OK button — musicA.13 geometry
                if !state.arrivalWaypoint {
                    RoundedRectangle(cornerRadius: 26, style: .continuous)
                        .fill(Color(white: 0.12))
                        .overlay(
                            RoundedRectangle(cornerRadius: 26, style: .continuous)
                                .stroke(Color.pink.opacity(pulse), lineWidth: 2)
                        )
                        .frame(width: TouchMapper.fxOkW, height: TouchMapper.fxOkH)
                        .overlay(
                            Text("OK")
                                .font(.system(size: 22, weight: .bold, design: .rounded))
                                .foregroundStyle(.white)
                        )
                        .position(x: 233, y: TouchMapper.fxOkY + TouchMapper.fxOkH / 2)
                }
            }
            .frame(width: 466, height: 466)
        }
    }

    private var tripLine: String {
        let dist = state.tripM >= 1000
            ? String(format: "%.1f km", Double(state.tripM) / 1000)
            : "\(state.tripM) m"
        return "\(dist)  \(state.tripMin) min"
    }
}
