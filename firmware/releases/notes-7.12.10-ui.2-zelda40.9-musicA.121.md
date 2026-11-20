# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.121

Shenhua boot: remove the logo fade-in, keep the lightning.

- The Shenhua logo is now drawn at full brightness from the very first frame; the previous 300 ms fade-in is removed.
- The subtle 500 ms scale settle is kept, and all lightning effects are unchanged: 14 main strikes, hit glows/sparks and the crawling edge arcs.
- The logo holds until 3600 ms and fades out over the final 400 ms; total boot duration stays 4000 ms.
- Center animations (supplied transparent sprites, gutter-based frame split) and WiFi fixes are unchanged from .120.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
