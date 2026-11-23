# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.130

Dual-slot firmware switching via boot-time power-key combo.

- Holding the power key for about 2 seconds while powering on switches the OTA boot slot and reboots into the other app firmware. The check runs before `pwrKeyInit()` clears the PMU key events.
- Add a second slot build (`musicA.130-slotB`) so both slots contain a bootable firmware and both can switch back.
- The slot switch uses `esp_ota_set_boot_partition()` + restart; `otadata` remains the source of truth.
- All other features (Shenhua boot, iOS home hero, WiFi flag clearing, screensaver time fix, auto-rotate direction) are unchanged.

Flash both app images: slot A (this release) at the device's ota_0 offset, slot B at ota_1. On the factory 32MB table used by some boards these are `0x20000` and `0x790000`; on this project's 16MB table they are `0x10000` and `0x410000`.

Flash app-only to keep NVS / BLE bonds.
