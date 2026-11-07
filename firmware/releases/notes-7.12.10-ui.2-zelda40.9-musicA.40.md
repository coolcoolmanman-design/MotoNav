# Firmware 7.12.10-ui.2-zelda40.9-musicA.40

Pairs with **iOS 0.16.2.38**. Draft-only self-test.

## Battery P0

- `drawTopSlot` wipe starts at **y=42** (was 32) so the larger status battery is not clipped.
- `animateCat` always calls **`drawStatus()`** after dynBg full-screen restore / slot redraw.
- Status battery icon enlarged (~40×18); erase band widened.

## Standby / settings IA (device side)

- DynBg + `csPal` always night palette (no day cream).
- `#FC2` mask bit1 no longer forced on (App battery toggle).
- `#FC3` top 0..5 (5=hide), center 0..3 (3=hide); bit3 follows center≠hide.
- Fullscreen: digital time+date above pivot (raised); B = near-center black circles then hands on top.
- Moon-phase style draws a visible moon aperture; center=time content inset away from side/top slots.
