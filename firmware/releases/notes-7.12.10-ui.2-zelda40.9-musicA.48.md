# Firmware 7.12.10-ui.2-zelda40.9-musicA.48

Field polish on musicA.46 GTA assets + includes musicA.47 heap tinfl fix. Pair with iOS **0.16.2.41** (no App change). Do not merge upstream. Draft only — no PR.

## Changes

- **Boot GTA VI (id 18):** denser **14 frames / ~3.0 s** (~214 ms/frame); same neon/flare phases as the mockup, smoother than A.46’s 9 frames.
- **Center vehId 3:** restore cleaner pixel art from field ref (`ref-standby-chars-v2`); sleep + chase scaled to **~240–250 px** content so outer slots / wallpaper keep margin (was ~345–395 px).
- **Includes musicA.47:** heap-allocated zlib inflater (fixes `loopTask` stack canary on GTA inflate).
- Keeps A.46 wallpaper/badge ffat persist + center-time second-tick fix.

## Size

| Asset | Frames | zlib |
|------:|-------:|-----:|
| Boot | 14 | ~244 KiB (48-color quant) |
| Sleep | 8 | ~60 KiB |
| Chase | 10 | ~72 KiB |
| **GTA PROGMEM total** | | **~373 KiB** (was ~524 KiB in A.46) |

App partition stays under **3 MiB** (est. ~2.25 MiB vs A.46 ~2.4 MiB).

## Baseline

- `cursor/musica47-gta-inflate-stack-6789` @ `915db09`
