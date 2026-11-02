# Firmware zelda40.9-musicA.3

Draft night-flash. Flash before IPA `0.16.2.5-tpms`. Option A UX unchanged.

## Root cause (why .2 still failed)

1. **Home clock / ticks**: iOS `0.16.2.1+` sends `#TIME,<utcSec>,<tzOffset>` (`PhoneClockSync`), but FW only parsed `#TIME,yyyy-MM-dd HH:mm:ss` → `clkSyncMs` stayed 0 → blank/`--:--` and no second ring.
2. **Blank cover + missing title**: `COVER_PX 240` + full-screen chrome (~434KB) + large type-5 bulk starved BLE image slots 5/6 and stressed PSRAM; chrome path could leave buttons-only frames.

## Fixes

- Accept unix `#TIME,<sec>,<offset>` (keep legacy wall string).
- `COVER_PX` back to **128**; keep COVER_HI 320 bilinear + fixed-point spin.
- Drop full-screen chrome cache; spin frames redraw title/artist + lite buttons.
- Idle: Zelda then clock+full second ring every tick; inset scene wipe.
