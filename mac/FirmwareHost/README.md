# FirmwareHost — Mac host build of Waveshare ESP32-S3 1.75C firmware

**What this is:** compile and run the real `firmware/boards/175/MotoNav` UI/protocol sources on macOS (or Linux) with a soft 466×466 RGB565 canvas. Edit firmware headers → rebuild → see the same draw path without flashing.

**What this is not:** the Swift RoundSim page clone. `mac/RoundSim` is kept only as an optional TCP framing reference. Day-to-day debug uses **this** host + DEBUG iOS.

Baseline tip branch: **`cursor/firmware-host-sim-unified-25f4`** (FirmwareHost + musicA.37 + DEBUG iOS RoundSimTransport).

## Single-branch checkout

```bash
git fetch origin cursor/firmware-host-sim-unified-25f4
git checkout cursor/firmware-host-sim-unified-25f4
```

## Deps (macOS)

```bash
brew install cmake sdl2
```

## Build & run FirmwareHost

```bash
cd mac/FirmwareHost
cmake -S . -B build            # finds Homebrew SDL2 when present
cmake --build build -j
./build/FirmwareHost
```

Listens on **`127.0.0.1:17875`**. Framing: `[u32 LE length][payload]` (max 500).

Headless (PPM only): `cmake -S . -B build -DHOST_HAS_SDL=OFF && cmake --build build -j`

## Point Xcode Simulator DEBUG iOS at FirmwareHost

1. Start `./build/FirmwareHost` on the Mac (window or headless).
2. Open this same branch in Xcode → **DEBUG** scheme → iOS Simulator.
3. Round screen Tab (or status card) → **Connect to Mac RoundSim**.
4. Simulator uses host `127.0.0.1` by default. Physical device: set UserDefaults `roundsim_host` to the Mac LAN IP.
5. Release / Draft production BLE path is untouched (`#if DEBUG` only).

### Inject without a phone

```bash
python3 - <<'PY'
import socket, struct, time
def frame(p: bytes): return struct.pack('<I', len(p)) + p
s = socket.create_connection(('127.0.0.1', 17875))
s.sendall(frame(b'#v,36'))
s.sendall(frame(b'#Z,1,Track,Artist'))
s.sendall(frame(b'A,0'))
s.sendall(frame(b'#O,12000,15,60'))
time.sleep(2)
s.close()
PY
```

## Layout

| Path | Role |
|--|--|
| `src/firmware_bundle.cpp` | Includes real `MotoNav.ino` under `HOST_SIM=1` |
| `src/host_present.*` | SDL window or headless PPM + circular clip |
| `src/host_tcp.*` | NimBLE stand-in ↔ RoundSim framing |
| `src/host_touch.*` | Mouse → CST raw (+ TOUCH_FLIP inverse) |
| `stubs/` | Arduino / GFX canvas / BLE / FreeRTOS / ESP shims |
| `../RoundSim/` | Optional Swift framing reference (not required) |

Full architecture + coverage: Project doc `docs/firmware-host-round-sim.md`.
