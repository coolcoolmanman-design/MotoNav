# MotoNav Mac RoundSim

Native **macOS 13+** SwiftUI tool that emulates a Waveshare **1.75″ / 466×466** circular screen over local TCP so DEBUG iOS builds can exercise the BLE text + bulk protocol without flashing firmware every time.

Not a full ESP32/LVGL emulator. Pixel parity is intentionally approximate.

## Open / run on Mac

```bash
cd mac/RoundSim
open Package.swift          # Xcode → Run (⌘R)
# or:
swift run RoundSim
```

Window shows the 466×466 clipped disc (optional dark bezel) and a side panel for inject / logs. TCP listen starts automatically on **`127.0.0.1:17875`**.

## Point DEBUG iOS at the sim

1. Run RoundSim on the Mac.
2. Build/run **DEBUG** MotoNav (Release has no RoundSim code).
3. On the Connect card tap **Connect to Mac RoundSim**.
   - **iOS Simulator** → host `127.0.0.1` (default).
   - **Physical iPhone** → set UserDefaults `roundsim_host` to the Mac LAN IP (same Wi‑Fi); allow inbound TCP 17875 in macOS Firewall if prompted.
4. Optional: `roundsim_port` (default `17875`).

Release / App Store / Draft production BLE path is unchanged (`#if DEBUG` only).

## TCP framing

| | |
|--|--|
| Bind | `127.0.0.1:17875` (IPv4 loopback; change port in UI/code if needed) |
| Frame | `[u32 little-endian length][payload]` |
| Payload | Exactly one BLE RX write **or** TX notify unit (UTF-8 text, or binary `'B'` / `'M'` packet) |
| Max payload | **500** bytes (matches `BleLink.MAX_WRITE`) |

On accept, RoundSim notifies like firmware capability probes:

```
U,1
CAP,BLD,1
CAP,HID,1
CAP,LYR,1
```

Bulk complete → `K,{type},{id}` (same as FW `BulkReceiver`), so iOS `MapFeeder` / cover pushes do not hang waiting for ACK.

## Manual inject

Side panel text field accepts one `#…` / nav line per line (e.g. `#v,42`, `R,120`, `A,0`, `#O12340/28/67`, `#Z,1,1,7`). Useful for FW-less UI checks.

## Pages (MVP)

| Page | Behavior |
|------|----------|
| Idle | Clock-ish home + ready/no-link |
| Music | Circular cover from bulk type 5 (`CVR`+zlib RGB565); tonearm lowers when `#Z` playing + `#RC` |
| Nav HUD | Direction / distance / speed / remain; map bulk types 1–4 **ACK + status stub** (no full raster decode) |
| Arrival | Celebration + **OK** hit (musicA.13 geometry `y≈372`, 168×52, pad 28). Mouse = canvas coords; toggle **Sim TOUCH_FLIP raw** to exercise the device flip path |

## Limitations vs real FW

- No LVGL/ESP32 timing, QSPI, battery, HID AMS bitstreams, traffic-light countdown, AutoSDK.
- Map/raster/buildings: ACK only; placeholder HUD.
- Text bitmaps (`B` slots): dimensions logged; not glyph-rendered.
- Music keys / swipe pages are simplified.
- Not pixel-identical to musicA.13 vinyl shaders.

## Protocol coverage (MVP)

See project report `docs/mac-round-sim.md` for the matrix.
