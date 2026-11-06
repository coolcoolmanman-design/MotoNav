# FirmwareHost — Mac host build of Waveshare ESP32-S3 1.75C firmware

**What this is:** compile and run the real `firmware/boards/175/MotoNav` UI/protocol sources on macOS (or Linux) with a soft 466×466 RGB565 canvas. Edit firmware headers → rebuild → see the same draw path without flashing.

**What this is not:** the Swift RoundSim page clone (`mac/RoundSim` on `cursor/mac-round-sim-4777`). RoundSim cannot import/run firmware sources; keep it only as a TCP framing reference (`127.0.0.1:17875`).

Baseline tip: **musicA.37** (`v7.12.10-ui.2-zelda40.9-musicA.37`), 微雪 1.75 / CO5300 / 466×466.

## Deps (macOS)

```bash
brew install cmake sdl2
```

Linux smoke (headless PPM) needs only a C++17 toolchain + CMake; SDL is optional.

## Build

```bash
cd mac/FirmwareHost
cmake -S . -B build            # finds Homebrew SDL2 when present
cmake --build build -j
```

Headless (no window; writes `/tmp/motonav_host.ppm`):

```bash
cmake -S . -B build -DHOST_HAS_SDL=OFF
cmake --build build -j
```

## Run

```bash
./build/FirmwareHost
```

- Window (with SDL): live circular-clipped 466×466 framebuffer from the real FW canvas.
- TCP: **`127.0.0.1:17875`**, framing `[u32 LE length][payload]` (max 500) — same as RoundSim / DEBUG iOS `RoundSimTransport`.
- Touch: mouse click → CST raw inject with **inverse of `TOUCH_FLIP_X/Y=1`** so Arrival OK / music hit-tests match device.
- Finite smoke: `FH_HOST_FRAMES=120 ./build/FirmwareHost`

### Inject without a phone

```bash
python3 - <<'PY'
import socket, struct, time
def frame(p: bytes): return struct.pack('<I', len(p)) + p
s = socket.create_connection(('127.0.0.1', 17875))
s.sendall(frame(b'#v,36'))
s.sendall(frame(b'#Z,1,Track,Artist'))
s.sendall(frame(b'A,0'))          # destination arrival (OK stays until tap)
s.sendall(frame(b'#O,12000,15,60'))
time.sleep(2)
s.close()
PY
```

### DEBUG iOS → host FW

1. Start `FirmwareHost` on the Mac.
2. DEBUG MotoNav → **Connect to Mac RoundSim** (same TCP transport).
3. Simulator: host `127.0.0.1`. Device: set `roundsim_host` to the Mac LAN IP.
4. Release/Draft BLE path unchanged.

## Layout

| Path | Role |
|--|--|
| `src/firmware_bundle.cpp` | Includes real `MotoNav.ino` under `HOST_SIM=1` |
| `src/host_present.*` | SDL window or headless PPM + circular clip |
| `src/host_tcp.*` | NimBLE stand-in ↔ RoundSim framing |
| `src/host_touch.*` | Mouse → CST raw (+ TOUCH_FLIP inverse) |
| `stubs/` | Arduino / GFX canvas / BLE / FreeRTOS / ESP shims |

Minimal FW guards (`HOST_SIM`): `DeviceContext.h` (Apple Media stub), `DisplayPipeline.h` (present FB), `IncomingCall.h` (touch IRQ), `BootAnimation.h` / `MotoNav.ino` (skip heavy boot assets / logo delay).

Full architecture, coverage matrix, and debug workflow: see the Project doc `docs/firmware-host-round-sim.md`.
