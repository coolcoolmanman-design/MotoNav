# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.122

Shenhua boot: logo is fully visible at full size from the first frame.

- Removed the remaining 500 ms scale settle (80% -> 100%) that made the logo look like it was still fading in after .121 removed the brightness fade-in.
- All lightning effects are unchanged: 14 main strikes, hit glows/sparks, crawling edge arcs and the blue full-screen flash on big strikes.
- Logo holds at full brightness and full size until 3600 ms, then fades out over the final 400 ms; total boot duration stays 4000 ms.
- Center animations and WiFi fixes are unchanged from .121.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
