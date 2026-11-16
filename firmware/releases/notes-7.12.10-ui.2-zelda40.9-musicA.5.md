# Firmware zelda40.9-musicA.5

Draft night-flash. Flash FW first; keep IPA `0.16.2.5-tpms` (COVER_PX still 128). Option A UX unchanged.

## Root cause (musicA.4 field retest)

1. **Spin smoothness regressed**: per-frame bilinear sample over the full 466 round (COVER_HI 400) was too heavy → dropped frames vs musicA.3 nearest rotate.
2. **Cover still soft**: 128→400 alone was not enough; spending CPU on rotate bilinear did not buy lasting sharpness.

## Tradeoff / fix

- **Keep** musicA.4 wins: second-ring 9×9 zbg clear; strip chrome blit above spin (no glass flicker).
- **Sharper cheaply**: `COVER_HI = LCD_WIDTH` (466). One-time bilinear upsample + light unsharp at CVR load; rotate uses **nearest-neighbor** only (scale≈1).
- **No IPA / no 240 CVR**: protocol stays 128 (last 240 starved title/cover).
- Keep musicA.3: unix `#TIME` parse; title/artist; Option A colors; no MUSIC title; glass keys.
