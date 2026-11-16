# Firmware 7.12.10-ui.2-zelda40.9-musicA.43

FW-only Draft on musicA.42. Do not flash unless asked. No iOS bump.

## Center = time · residual wipe clip

- Steady `animateCat`: when `centerFunc==time`, **omit** post-draw top / TL / TR `zbgRect` residual clears (they ran after `drawCenterClock` and clipped the enlarged block into y42–136 / TL-TR zones).
- Redraw `drawCenterClock()` once after the bottom slot so z-order stays clean.
- `drawWaiting`: skip the one-shot `drawTopSlot()` when center=time (avoids rebuild flash).

## Fullscreen B · four function circles

- `circleR` **33 → 56** (~+70%); `centerRadial` **stays 118** (no outward move).
- Layer order unchanged: face → black circles → solid battery+% → hands on top.
- Circles must not cover battery+%.

## Baseline

- `cursor/dial-clip-fullscreen-fix-34df` @ `64e3026` (musicA.42 / iOS 0.16.2.40)
