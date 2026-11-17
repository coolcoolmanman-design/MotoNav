# MotoNav 固件 musicA.89 · 申花 B hero + 体育场连接态

> 配套 iOS **0.16.2.83**。基线 musicA.88。**musicA.84 仍不安全，勿刷。** Draft 保持。

## 本版

- **Idle：** 确认稿 B 全幅重绘 `hero-idle` 01–08（无 idle-02 悬浮标）。
- **Connected：** 确认稿体育场草坪场景 `shenhua-stadium-scene-mock/frames`（opaque over grass · 清晰车标 · 趴地红人同朝向）。
- 保留 soft outer rim / hard body、~189×135（≈85–90%）、disconnect→idle（shownConn / `#OFF`）、boot AA。
- 体育场场景进**中央连接态动画**（非独立 dynBg 新风格；dynBg 0..7 仍程序化）。
- Flash：本地 `Sketch 3131683` · `app.bin 3131936` · 分区余量 **13792 B**（≈13.5 KiB ≥1.5 KiB）。

## 版本

- FW `7.12.10-ui.2-zelda40.9-musicA.89`
- iOS `0.16.2.83`
