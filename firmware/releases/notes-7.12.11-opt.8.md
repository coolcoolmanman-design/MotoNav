# 7.12.11-opt.8

Stack: opt.7 tip (map/NACK + soft-blob lineage) with **opt.5 square cover restored** as the primary music look.

## Changes

- Music cover: restore opaque centered `drawCoverSquare` (side 180 @ cy=118, light round-rect). Remove NetEase full-bleed gray atmosphere and opt.7 soft gray blob as the main look. No full-bleed photo/theme gray bg.
- Progress times: keep opt.5+ elapsed/total **inset** (`y+12`, horizontal inset 32).
- `parseCover`: keep Deflate length loosen (`len ≥ 6+8`); **skip** gray force and Bayer/blur preprocess — display color when the phone sends color.
- iOS `CoverBridge` source: back to **128×128 color** RGB565 (same CVR v1). **IPA not cut** this Draft.

## Not in this pack

PR open/merge, flash, IPA bump, Android encode, Zelda UI, GPS/stopwatch, publish release.
