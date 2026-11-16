# Firmware 7.12.10-ui.2-zelda40.9-musicA.41

Pairs with **iOS 0.16.2.39**. Draft-only self-test. Do not flash unless asked.

## Fullscreen pointer (中央=全屏指针)

- Delete digital time + date above pivot.
- Delete outermost second-hand tick ring (`drawRingTicks` no longer called; `hasTimeFace` excludes `centerFunc==2`).
- True edge-to-edge hands (`r≈214/226`, longer ratios).
- Solid battery with centered % drawn **above pivot**; hands layered on top (Style B: near-center black func circles → battery → hands).
- Four function slots still short-circuited (unchanged).

## Center = time

- Hide top / TL / TR on device; keep bottom slot.
- Enlarge central digital time / mini dials.
- Mini/inset pointer dials: no digital time/date under or on the dial; moon-phase graphic kept.

## Battery

- `drawBattery` solid fill + dark centered percentage (fixes hollow regression for standby + map + fullscreen).

## App IA companion

- iOS 0.16.2.39: four-tab settings shell, two-row conn/batt, greyouts for center=time / fullscreen.
