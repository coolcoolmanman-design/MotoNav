# Firmware 7.12.10-ui.2-zelda40.9-musicA.42

Pairs with **iOS 0.16.2.40**. Draft-only self-test. Do not flash unless asked.

## Center = time

- Move time block **up** to true round-screen center `(CX,CX)`.
- Enlarge digital styles and mini dials (`ar` 96→118); clear band expands above former top-slot area (still clears above bottom slot).
- Avoid circular-bezel clipping of time edges after top/TL/TR are force-hidden.

## Fullscreen pointer

- Style **B 外环** uses the same true edge-to-edge radius as **A 纯净** (`r=226`; no longer one ring smaller).
- Four function black circles slightly larger (`28→33`) and radially nudged out (`108→118`) so they do not cover battery+% / pivot.

## App companion

- iOS 0.16.2.40 restores 表盘样式 + 指针样式 dropdowns under 中央=全屏指针.
