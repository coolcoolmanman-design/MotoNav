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

## Install identity (overwrite-install)

| Field | Required value |
| --- | --- |
| applicationId / Package | `com.motonav.u25b1bbb7d6394a8593b54c6d2b5f09a8` |
| Signing cert SHA1 | `F7:1F:9D:21:AA:96:E7:61:22:13:9C:7C:83:C7:DB:5D:0B:1F:DD:5D` |

Pinned in `android/gradle.properties` as `personalApplicationId`. Release signing still needs the **same** personal JKS that produces that SHA1 (see `Build-Personal.ps1`: `android/keystore/personal.jks` + `android/keystore/keystore.properties`, alias `motonav`). **Do not generate a replacement keystore.**

## Assets

Sanitized Android source zip (no keystore, no personal Keys). Signed APK upload is blocked until that JKS is available to CI/agent (Cursor secret or restored `android/keystore/`).

## Safety

Do not ride while operating the phone. Follow traffic law. No map/weather Keys are bundled.
