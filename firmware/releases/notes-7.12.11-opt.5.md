# Firmware 7.12.11-opt.5

Stack: opt.4 (near-empty basemap NACK) + **square cover** + **readable duration labels**.

## Changes

- Music cover: stop vinyl `drawCoverDisc`; draw centered square (light rounded-rect, side 180) from type-5 128×128 RGB565. No circular mask by default.
- Progress times: draw elapsed/total **above** the bar with **28px horizontal inset** so round-bezel chord no longer clips digits; page dots stay clear at y=444.
- `parseCover`: drop `len ≥ 6+w*h*2` gate so real Deflate covers are accepted (store-block workaround no longer required). tinfl still validates full decode.

## Pairing

Use with iOS **0.15.17-tpms** (or later with gray-road + empty-mask fixes). Firmware-only UI; no IPA change required for cover/time.
