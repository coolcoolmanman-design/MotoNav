# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.124

Shenhua boot: crest fitted to the round screen with short fades.

- The crest boot badge is reduced from 466x466 to 310x310 centred in the 466x466 canvas. Its half-diagonal is ~219 px, inside the 233 px round-screen radius, so no part is clipped by the circular display.
- Added a short 250 ms fade-in and a short 250 ms fade-out (3750-4000 ms). The previous version had no fades; this is the requested shorter trial.
- The crest stays at a fixed size throughout; all motion remains the live lightning: 14 main strikes, hit glows/sparks, crawling edge arcs, micro bolts and blue bloom.
- Total boot duration stays 4000 ms. Center animations and WiFi fixes are unchanged.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
