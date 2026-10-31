# Firmware 7.12.11-opt.7

Stack: opt.6 (gray atmosphere) + **LCD-friendly cover compromise** (less banding/mosaic, faster appear).

## Changes

- Music page draws a **fixed dark radial theme gradient immediately** (placeholder; no photo sample).
- Cover is no longer full-bleed NN-upsampled gray photo every redraw.
- After `parseCover` Deflate: gray → **box downscale to 64²** → **3×3 blur** → **16-level Bayer dither**; draw as a **dim center soft blob** (~Ø220) with edge fade.
- iOS `CoverBridge`: encode **64×64 grayscale** (was 128) to shrink BLE; same `CVR` v1 / type5. **IPA not cut** this Draft — phone still sending 128 works (device downscales).
- Soft glass controls / progress chrome from opt.6 kept.

## Pairing

Firmware alone improves banding vs opt.6 full-bleed. Full latency win needs iOS build with 64px `CoverBridge`. Android CoverBridge size deferred.
