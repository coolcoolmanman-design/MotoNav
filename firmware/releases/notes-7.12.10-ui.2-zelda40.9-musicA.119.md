# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.119

Shenhua animation cleanup and boot timing polish.

- Re-cut both Shenhua center animations (idle / connected) from the supplied strips. Only the rider and motorcycle remain opaque; background and stray fragments are fully transparent. All 8 frames per set stay at 189x135.
- Removed near-white background speckles that showed through as a checkerboard. Enclosed white bodywork is filled back in, and there are no enclosed transparent holes inside the subject.
- Shenhua boot animation now holds for 4000 ms total, with the fade-out starting at 3400 ms (was 3200 / 2900 ms). The other boot brands are unchanged.
- Frame geometry and the 2x centered placement of the center animation are unchanged, as are the WiFi SoftAP / map raster fixes from .117 and .118.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
