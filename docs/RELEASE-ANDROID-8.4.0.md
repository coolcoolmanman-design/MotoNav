# Android 8.4.0 · iOS tip phone-feature port + Fold8

Date: 2026-11-22. versionName `8.4.0`, versionCode `140`.

Aligned with iOS tip **0.16.2.108** / musicA.129 phone-client features (not firmware bake). SoftAP kept because tip still enables Wi‑Fi map boost.

## Highlights

- Four tabs: **导航 / 行程 / 圆屏 / 我的** (cool gray + system blue).
- Appearance: auto / manual light / manual dark.
- Trips overview (week/month/year) with Fold wide split.
- Wi‑Fi SoftAP map acceleration toggle + BLE `WIFI` status.
- Auto upright rotation (`#AR`).
- Tianditu map source restored (`TdtRaster` + Key field).
- Fold8: inner unfolded dual-pane / landscape side rail; cover side rail heuristic.
- Nav HUD translucent bottom bar; TPMS + ConnectAnim retained from 8.3.27.

## Assets

Sanitized Android source zip only (no keystore, no personal Keys, no APK credentials). Build with JDK 17 + Android SDK 35 under `android/`.

## Safety

Do not ride while operating the phone. Follow traffic law. No map/weather Keys are bundled.
