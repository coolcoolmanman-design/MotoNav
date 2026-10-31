# 7.12.11-opt.10

Stack: opt.9 tip (despeckle + near-empty NACK + DFL/DXR + type5 square cover) with mid-speed blank-tile defense.

## Changes

- **DXR overlap**: reject shifts with &lt;~67% overlap (`width/3`, was `width/2`). Mid-speed large-shift DXR was still accepted under 50%, then XOR-desynced → NACK storms → blank tiled blocks while the phone retried. Failed DXR still keeps the last dense tile (`rasterValid` uncleared).
- **Despeckle**: one pass, and only when the plane is ≥2× the near-empty floor — avoids scrubbing thin mid-speed pavement into a near-empty NACK (stale holes).
- Near-empty NACK gate unchanged (still rejects near-all-land over a dense prior / empty first tile).
- Music: still **type5** square cover + duration inset. No type6 / stopwatch / FLAG_*.

## Pairing

Flash with iOS **0.15.28-tpms** (plain-RLE gate 55 + DFL-only from 40). Prefer app @ `0x10000`.

## Not in this pack

PR open/merge, stopwatch board, FLAG_*/cover type6, upstream merge, undraft release.
