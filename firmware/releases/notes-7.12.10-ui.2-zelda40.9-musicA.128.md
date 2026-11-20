# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.128

Fix the top time slot showing during the screensaver.

- `drawIdleClock()` and `drawTopSlot()` now return immediately while `saverActive`, so phone time syncs and page redraws can no longer paint the date/time into the screensaver framebuffer.
- `#TIME` and `#C` message handlers also skip redrawing the waiting page during the screensaver; only the eye animation is refreshed.
- The status bar path already had this guard; the top time path was missing it.
- Boot/center animations, crest and auto-rotate direction fix are unchanged from .127/.126.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
