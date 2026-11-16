# Firmware 7.12.10-ui.2-zelda40.9-musicA.44

FW-only Draft on musicA.43. Do not flash unless asked. No iOS bump.

## Fullscreen B · four function circles

- `centerRadial` **118 → 133** (+15 px toward bezel; small nudge, not back to 168).
- `circleR` **stays 56** (musicA.43 +70%).
- Labels: Chinese `速度/实时G/胎压/天气` → ASCII **SPD / LIVE G / TPMS / WX**.
  - Root cause: `printCentered` uses Adafruit built-in font (ASCII only); UTF-8 CJK was drawn byte-wise → mojibake. Digits were fine.
- Layer order unchanged: face → black circles → solid battery+% → hands on top.

## Baseline

- `cursor/time-wipe-b-circles-4f7b` @ `7af4511` (musicA.43 / iOS 0.16.2.40)
