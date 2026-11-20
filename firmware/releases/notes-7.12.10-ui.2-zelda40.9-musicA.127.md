# MotoNav firmware 7.12.10-ui.2-zelda40.9-musicA.127

Shenhua boot crest: restore all white areas inside the logo.

- Undo the white-band removal from .125: every white region enclosed by the crest (including the top band above the shield) is kept opaque again. Only the area outside the crest is transparent/black.
- Keep the soft antialiased edges and the thin cool-white rim glow from .125.
- Keep the 310x310 centred crest so it stays inside the round screen, no fades, 4000 ms total, and the live lightning unchanged.
- Auto-rotate direction fix from .126 is preserved.

Flash app-only at `0x10000` to keep NVS / BLE bonds.
