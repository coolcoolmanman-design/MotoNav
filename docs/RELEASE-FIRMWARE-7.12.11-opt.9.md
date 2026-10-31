# 7.12.11-opt.9

Stack: opt.8 tip (square cover + near-empty NACK + DFL/DXR) with round-map dither defense.

## Changes

- Basemap decode: after RLE/DFL/DXR unpack, **despeckle isolated road nibbles** (no orthogonal road neighbor, 2 passes). Clears AA water-fringe / mid-gray false-road speckles that painted as dark Vice City `C_ROADM` dots on cream land. Connected pavement / park / water kept.
- Near-empty NACK gate unchanged (still rejects near-all-land tiles over a dense prior).
- Music: still **type5** square cover + duration inset 32 (opt.8 lineage). No type6 / stopwatch cover path.

## Pairing

Flash with iOS **0.15.23-tpms** (classifier tighten + empty-mask retained). Prefer app @ `0x10000`.

## Not in this pack

PR open/merge, stopwatch board, FLAG_*/cover type6, upstream merge, undraft release.
