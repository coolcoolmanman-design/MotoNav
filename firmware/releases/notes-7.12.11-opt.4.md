# Firmware 7.12.11-opt.4

Stack: opt.3 (music layout + DXR ≥50% NACK) + **empty basemap NACK**.

## Why

Field photo at ~120 km/h: round screen showed solid theme land (Vice City cream) with only the purple nav route + HUD. That is an all-land / empty material raster (or an empty pack displacing a good one), not a partial off-tile window.

## Change

- After DFL / DXR / RLE decode, count non-zero nibbles.
- If the new plane is near-empty and would replace a denser tile (or install as the first tile), **NACK** without clearing `rasterValid`.
- Phone (0.15.17) should stop sending empty road masks; this gate is defense in depth.

## Pairing

Use with iOS **0.15.17-tpms** (measured `backgroundLum` + gray highway roads + highway plain RLE).
