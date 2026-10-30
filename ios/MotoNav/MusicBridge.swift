import Foundation
import MediaPlayer

/// HID controls the active player; AMS metadata arrives from the round screen.
/// The optional App control mode uses the system Music player.
final class MusicBridge {
    static let shared = MusicBridge()
    private lazy var player = MPMusicPlayerController.systemMusicPlayer
    private var observing = false
    private var lastText = ""
    private var lastState = ""
    private var lastBase = ""
    private var lastProgressElapsedMs: Int?
    private var lastProgressAt: TimeInterval?
    private var lyricsSync = LyricsDisplaySync()
    private(set) var metadata = AMSMetadata()
    func receiveAMS(_ data: Data) {
        guard metadata.receive(data) else { return }
        BleLink.shared.mediaSongInfo = metadata.summary
        refresh()
    }
    func resetAMS() {
        metadata.reset(); lyricsSync.reset(); LyricsBridge.shared.stop()
        lastText = ""; lastState = ""; lastBase = ""
        lastProgressElapsedMs = nil; lastProgressAt = nil
        CoverBridge.shared.reset()
        BleLink.shared.mediaSongInfo = ""
    }
    func syncMode() {
        guard BleLink.shared.mediaHIDSupported else { return }
        BleLink.shared.sendOnce(Settings.shared.musicHID ? "#HM,1" : "#HM,0")
        syncLyrics()
    }
    func syncLyrics(force: Bool = false) {
        let desired = Settings.shared.musicLyrics && Settings.shared.musicHID
        let changed = lyricsSync.desired != desired
        lyricsSync.configure(desired, force: force)
        if !desired { LyricsBridge.shared.stop() }
        guard let command = lyricsSync.command(now: ProcessInfo.processInfo.systemUptime, ready: BleLink.shared.isReady || force || changed) else { return }
        BleLink.shared.sendLyricsMode(command)
    }
    func lyricsReply(_ enabled: Bool) { lyricsSync.acknowledge(enabled); syncLyrics() }
    func authorize() {
        MPMediaLibrary.requestAuthorization { _ in
            DispatchQueue.main.async { self.refresh(force: true) }
        }
    }
    func refresh(force: Bool = false) {
        if Settings.shared.musicHID {
            LyricsBridge.shared.update(metadata, force: force)
            let ready = BleLink.shared.mediaHIDSupported && BleLink.shared.mediaHIDReady && BleLink.shared.mediaHIDMode
            if metadata.ready {
                let display = metadata.title.isEmpty ? "等待播放器歌曲信息" : metadata.title + (metadata.values[0].partial ? "…" : "")
                let artist = metadata.artist
                // Cover search must use the raw AMS title — display "…" breaks iTunes matching.
                publish(title: display, artist: artist, available: !metadata.title.isEmpty, playing: metadata.playing == true,
                        force: force, coverTitle: metadata.title, coverArtist: artist)
            } else {
                publish(title: "蓝牙媒体按键", artist: ready ? "请先在网易云或 QQ 音乐播放歌曲" : "请在 iPhone 蓝牙设置完成配对", available: false, playing: false, force: force)
            }
            return
        }
        LyricsBridge.shared.stop()
        guard MPMediaLibrary.authorizationStatus() == .authorized else {
            publish(title: "请在手机授权 Apple Music", artist: "仅支持系统音乐 App", available: false, playing: false, force: force)
            return
        }
        if !observing {
            observing = true
            player.beginGeneratingPlaybackNotifications()
            NotificationCenter.default.addObserver(self, selector: #selector(changed), name: .MPMusicPlayerControllerNowPlayingItemDidChange, object: player)
            NotificationCenter.default.addObserver(self, selector: #selector(changed), name: .MPMusicPlayerControllerPlaybackStateDidChange, object: player)
        }
        let item = player.nowPlayingItem
        publish(title: item?.title ?? "请先在音乐 App 播放歌曲", artist: item?.artist ?? "Apple Music", available: item != nil, playing: player.playbackState == .playing, force: force, item: item,
                coverTitle: item?.title ?? "", coverArtist: item?.artist ?? "")
    }
    @objc private func changed() { DispatchQueue.main.async { self.refresh() } }
    private func publish(title: String, artist: String, available: Bool, playing: Bool, force: Bool, item: MPMediaItem? = nil,
                         coverTitle: String? = nil, coverArtist: String? = nil) {
        let have = available ? 1 : 0, play = playing ? 1 : 0, mask = available ? 7 : 0
        let text = title + "\n" + artist
        let base = MusicZMessage.base(have: have, play: play, mask: mask)
        let progress = progressMs(item: item, available: available)
        let state = MusicZMessage.build(have: have, play: play, mask: mask,
                                        durationMs: progress?.durationMs, elapsedMs: progress?.elapsedMs)
        let trackChanged = force || text != lastText
        let seeked = progress.map {
            MusicZMessage.seeked(previousElapsedMs: lastProgressElapsedMs, previousAt: lastProgressAt,
                                 now: ProcessInfo.processInfo.systemUptime, newElapsedMs: $0.elapsedMs, playing: playing)
        } ?? false
        if trackChanged {
            lastText = text
            for (slot, value) in [(5, title), (6, artist)] {
                let im = TextImage.render(String(value.prefix(200)), maxW: 360, size: slot == 5 ? 32 : 24, minSize: 18)
                BleLink.shared.sendImage(slot: slot, w: im?.w ?? 0, h: im?.h ?? 0, data: im?.data ?? Data())
            }
        }
        // PR5: append duration/elapsed when known; resync on seek/track change — no per-tick BLE progress.
        if force || base != lastBase || trackChanged || seeked {
            lastBase = base
            lastState = state
            BleLink.shared.send(state)
            if let progress {
                lastProgressElapsedMs = progress.elapsedMs
                lastProgressAt = ProcessInfo.processInfo.systemUptime
            } else {
                lastProgressElapsedMs = nil; lastProgressAt = nil
            }
        }
        // Cover: every available session (App artwork / HID library+iTunes). Use clean titles.
        if available {
            CoverBridge.shared.update(title: coverTitle ?? title, artist: coverArtist ?? artist, item: item)
        } else {
            CoverBridge.shared.update(title: "", artist: "", item: nil)
        }
    }
    /// AMS duration is seconds in field 3; App mode uses MPMediaItem + player clock.
    private func progressMs(item: MPMediaItem?, available: Bool) -> (durationMs: Int, elapsedMs: Int)? {
        guard available else { return nil }
        if Settings.shared.musicHID {
            guard let durSec = Double(metadata.values[3].text), durSec.isFinite, durSec > 0, durSec <= 24 * 3600 else { return nil }
            let now = ProcessInfo.processInfo.systemUptime
            let elapsedSec: Double
            if let clock = MediaPlaybackClock(metadata.values[4].text, now: now) {
                elapsedSec = clock.position(now: now, duration: durSec)
            } else { elapsedSec = 0 }
            return (Int((durSec * 1000).rounded()), Int((elapsedSec * 1000).rounded()))
        }
        guard let item, item.playbackDuration.isFinite, item.playbackDuration > 0 else { return nil }
        let durMs = Int((item.playbackDuration * 1000).rounded())
        let elapsed = player.currentPlaybackTime
        let elapsedMs = elapsed.isFinite && elapsed >= 0 ? Int((elapsed * 1000).rounded()) : 0
        return (durMs, min(elapsedMs, durMs))
    }
    func handle(_ action: String) {
        if action == "Q", Settings.shared.musicHID { BleLink.shared.sendOnce("#AMS?") }
        refresh(force: action == "Q")
        guard ["P", "N", "T"].contains(action) else { return }
        // A delayed legacy App event must not also control Music while HID is selected.
        guard !Settings.shared.musicHID else { return }
        guard MPMediaLibrary.authorizationStatus() == .authorized, player.nowPlayingItem != nil else {
            BleLink.shared.sendOnce("#ZACK,5"); return
        }
        BleLink.shared.sendOnce("#ZACK,2")
        let before = player.nowPlayingItem?.persistentID, wasPlaying = player.playbackState == .playing
        switch action {
        case "P": player.skipToPreviousItem()
        case "N": player.skipToNextItem()
        default: wasPlaying ? player.pause() : player.play()
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
            let changed = action == "T" ? (self.player.playbackState == .playing) != wasPlaying : self.player.nowPlayingItem?.persistentID != before
            BleLink.shared.sendOnce(changed ? "#ZACK,3" : "#ZACK,4")
            self.refresh(force: true)
        }
    }
}
