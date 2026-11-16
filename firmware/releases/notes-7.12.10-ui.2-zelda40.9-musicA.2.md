# Firmware zelda40.9-musicA.2

Draft night-flash. Flash this before pairing IPA `0.16.2.3` (240 CVR). Option A UX unchanged.

## Fixes

- **Music cover spin**: COVER_HI 320 bilinear upsample once on parse; 16.16 fixed-point rotate; chrome overlay cache + blit on spin frames (no per-frame glass/text thrash). ~36s/rev, ~20fps.
- **Mosaic**: Accept CVR up to 240 (still compatible with 128); sharper scale into full-bleed.
- **Home clock / second ticks**: `animateCat` draws Zelda scene first, then clock + full second ring each tick (scene band was erasing 3/9 tick marks). `slideCommit` to home forces `onWaitScreen` + `drawWaiting` so time rebinds after leaving music.

## Keep

Option A 冷雾银 colors, no「MUSIC」title, glass keys, fixed overlay.
