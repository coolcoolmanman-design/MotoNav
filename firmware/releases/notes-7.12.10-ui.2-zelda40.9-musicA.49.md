# Firmware 7.12.10-ui.2-zelda40.9-musicA.49

Field fixes on musicA.48 (GTA center wipe / Zzz / chase margins / second ticks / power menu). Pair with iOS **0.16.2.41** (no App change — power menu is FW-only). Do not merge upstream. Draft only — no PR.

## Changes

- **GTA center composite:** inflate to PSRAM scratch, chroma-key `0` → keep dynBg/custom wallpaper (A.48 full-FB blit tore wallpaper with opaque black).
- **Smaller center art:** sleep/chase content boxed to **~163×(92–181)** inside safe zone clear of L/R slot wipes and below top-clock / Zzz overlap (`y≥150`).
- **Second ticks:** redraw `drawSecondRing` after func slots + status (bottom/L/R wipes were eating diagonal ticks); tighten center-time wipe to `CX±140, y76..316`.
- **Power menu:** **Sleep → Restart** (label + `ESP.restart()`; no deep sleep).

## Size

| Asset | Frames | zlib |
|------:|-------:|-----:|
| Sleep | 8 | ~59 KiB |
| Chase | 10 | ~67 KiB |
| Boot (unchanged A.48) | 14 | ~244 KiB |
| **GTA PROGMEM total** | | **~370 KiB** |

App partition stays under **3 MiB**.

## Baseline

- `cursor/musica48-gta-anim-polish-1c96` @ `3e7707d`
