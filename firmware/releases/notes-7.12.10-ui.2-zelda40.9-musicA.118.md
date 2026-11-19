# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.118

Fix for the map regression seen after .117 finally made WiFi 地图加速 usable.

- WiFi raster tiles now cancel any in-flight BLE raster work before decoding. BLE and WiFi previously could decode into the same back buffer, producing missing building outlines or a blank map.
- Changing map style (`#t,<style>,1`) or the building-layer switch (`#BC,0/1`) now drops any in-flight WiFi TCP raster tile, so a stale tile from the old style cannot overwrite the new style's map.
- Added serial logs for WiFi raster accept/reject, including theme, building-carry state and the resulting building flag.
- WiFi driver pre-init, SoftAP lifecycle and the NVS gate for non-WiFi users are unchanged from .117.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
