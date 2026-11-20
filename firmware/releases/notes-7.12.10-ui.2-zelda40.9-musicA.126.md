# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.126

Fix auto-rotate direction on the round panel.

- `AROT_DIR` is now -1. With +1 the computed target followed the physical device rotation in the same direction, so the content turned with the screen instead of staying upright; the firmware comment already documented this fallback.
- The anchor now uses the same round-to-nearest-90-degree rule as the poll loop and the disable path, so enabling auto-rotate from a non-90-degree manual angle no longer jumps a quadrant.
- Boot animation and center animations are unchanged from .125.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
