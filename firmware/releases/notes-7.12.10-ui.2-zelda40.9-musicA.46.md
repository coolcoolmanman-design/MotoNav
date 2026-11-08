# Firmware 7.12.10-ui.2-zelda40.9-musicA.46

Draft on musicA.45. Pair with iOS **0.16.2.41**. Do not merge upstream. Flash later if asked.

## A) Center=time · second-tick occlusion

- **Cause:** musicA.43 drew `drawCenterClock()` *after* `drawSecondRing`, but `drawCenterClock` still did a full-width `zbgRect(0,48,466,300)` that erased `SEC_RING_R=222` ticks on the left/right.
- **Fix:** Shrink center wipe to `zbgRect(CX-150,70,300,260)` and redraw `drawSecondRing` after the center clock when `centerFunc==1`.

## B) GTA VI boot + center loops

- Boot brand **18 = GTA VI** (9 zlib RGB565 frames, ~3.0 s); **Off = 19**.
- Center anim vehId **3 = GTA VI**: disconnected → sleep loop ~3.5 s (8f); connected → chase ~2.25 s (10f).
- Packaging: zlib frames in PROGMEM (`Gta6BootFrames.h` / `Gta6CenterSleep.h` / `Gta6CenterChase.h`) — ~536 KiB; app stays under 3 MiB (`app3M_fat9M_16MB`).

## C) Wallpaper + 电子吧唧 Flash persistence

- Mount existing **ffat**; on bulk type **8** / **7** success write `/bg/custom.bgr` / `/badge/slotN.bgr`.
- Boot load after boot anim, before first `drawWaiting`.
- Auth sends `BGR,cached,1` / `BDG,cached,<mask>` **before** `CAP,BGR` / `CAP,BDG` so App skips forced resend; clear/update paths unchanged.

## Baseline

- `cursor/b-circle-label-size-1fb0` @ `02cc876` (musicA.45)
