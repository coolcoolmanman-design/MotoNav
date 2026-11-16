# Firmware 7.12.10-ui.2-zelda40.9-musicA.45

FW-only Draft on musicA.44. Do not flash unless asked. No iOS bump.

## Fullscreen B · enlarge labels & values inside discs

- `circleR` **stays 56**; `centerRadial` **stays 133**.
- Labels (SPD / LIVE G / TPMS / WX) and numeric values enlarged vs musicA.44 (`printCentered` sizes were 1 / 2).
- Fit-to-circle: Adafruit 6×8 glyph metrics; pick largest label/value sizes that stay inside the black disc with pad=6 (no overflow past the disc edge).
- Typical: label size **2**, value size **3–4** (falls back for long strings like `LIVE G` / `12.34`).
- Layer order unchanged: face → black circles → solid battery+% → hands on top.

## Baseline

- `cursor/b-circles-outward-garbled-edc4` @ `5796123` (musicA.44)
