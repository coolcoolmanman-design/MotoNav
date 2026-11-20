# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.120

Shenhua center animation rebuilt directly from the supplied transparent sprites.

- Both idle and connected animations now use the supplied RGBA sprites as-is: 8 frames each, 189x135, no cutout, hole filling or other image processing.
- Frame boundaries are detected from the transparent gutters in the strips instead of an even width/8 split. This fixes a frame edge artifact where the right side of a frame was cut off and the cut-off slice appeared at the left edge of the following frame.
- Shenhua boot logo fades in over 300 ms (was 1000 ms) with a 500 ms settle (was 720 ms), and holds until 3600 ms before fading out (was 3400 ms). Total boot duration stays 4000 ms.
- WiFi SoftAP / raster fixes from .117 and .118 are unchanged.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
