# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.129

Clear the sticky WiFi AP flag when the map boost is turned off.

- `wifiMapSetEnabled(false)` now clears the NVS `wifi_ap` flag. Previously the flag was only set to true on first enable and never cleared, so every later boot preinitialised the WiFi driver before BLE even when the feature was off (radio stopped, but driver/power domain still resident).
- With the flag cleared, a later re-enable still uses the existing save-flag + reboot + preinit path, so first-use reliability is unchanged.
- Screensaver top-time fix, auto-rotate direction and Shenhua boot crest are unchanged from .128/.127/.126.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
