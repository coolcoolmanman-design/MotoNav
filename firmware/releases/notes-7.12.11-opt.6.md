# Firmware 7.12.11-opt.6

Stack: opt.5 (square cover + time inset + parseCover Deflate loosen) + **NetEase-style gray atmosphere music UI**.

## Changes

- Music cover: stop opaque centered sticker; draw **full-bleed desaturated / dimmed cover** as soft vignette background (controls + title overlay).
- Transport chrome: soft dark glass discs + muted progress fill (no flat teal bricks).
- Elapsed/total labels: keep opt.5 inset; slightly brighter for readability on atmosphere bg.
- `parseCover`: after decode, **force grayscale** so older color phones still get soft bg. Protocol still `CVR` v1 RGB565 type5 128×128 (no flag).
- iOS `CoverBridge`: encode **grayscale RGB565** before zlib (shrinks BLE payload). Same branch; IPA not cut this Draft.

## Pairing

Prefer firmware alone with gray-on-device if phone still sends color. Full gray-transmit benefit needs iOS build that includes this `CoverBridge` change (next IPA). Android CoverBridge gray deferred.
