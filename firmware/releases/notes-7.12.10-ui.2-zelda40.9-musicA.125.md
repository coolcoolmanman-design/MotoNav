# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.125

Shenhua boot: no fades, cleaner crest edges, no white background bands.

- Removed the short fade-in/fade-out added in .124: the crest is drawn at full brightness from the first frame until the end of the 4000 ms animation.
- Kept the 310x310 centred crest so it fits inside the round screen.
- Removed the wide white background bands around the crest while leaving the white text and stripes inside the logo untouched.
- Softened the cutout edges with antialiased alpha premultiplied onto black, and added a thin cool-white rim glow so the logo reads cleanly on the dark round display.
- Lightning unchanged: 14 main strikes, edge arcs, micro bolts and blue bloom.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
