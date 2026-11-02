# Firmware zelda40.9-musicA.4

Draft night-flash. Flash FW first; keep IPA `0.16.2.5-tpms` (COVER_PX still 128 — no protocol bump). Option A UX unchanged.

## Root cause (musicA.3 field retest)

1. **Home second-hand ghost dots**: `drawSecondRing` only painted 3–5px grey ticks over prior 9×9 red tip → red corners left a trail of dots around the ring.
2. **Glass controls flicker while cover spins**: spin frames redrew lite/glass buttons each frame under/with the rotating blit → chrome shimmered.
3. **Cover still soft**: COVER_HI 320 nearest rotate from 128 still looked soft on the 466 round.

## Fixes

- Each ring tick: `zbgRect` 9×9 clear then grey tick (same erase path as tip move).
- Strip chrome cache (`MUSIC_CHROME_Y0=220`, ~229KB): dirty frames capture overlay; spin frames blit static chrome above rotate+scrim (no per-frame glass redraw).
- `COVER_HI` **400** + bilinear sample on rotate; protocol stays **128** (avoid 240 bulk / title blanking).
- Keep musicA.3: unix `#TIME` parse; title/artist slots; Option A colors; no MUSIC title; glass keys.
