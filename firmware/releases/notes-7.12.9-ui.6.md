# MotoNav universal firmware 7.12.9-ui.6

## New

- **Swipe up from home: G-FORCE / speedometer page** (pixel style). Top shows a large live G readout, center is a speedometer dial (progress arc, 270° full scale = 180 km/h) with a large speed figure, side columns show PEAK G / RIDE time and MAX speed / battery, plus an oscilloscope-style G trace at the bottom. Tapping ZERO clears the trace, RESET clears the peak.
- **Swipe down from home: TPMS tire-pressure page** (pixel style). Detailed pixel sport bike with rotating wheel spokes, per-wheel pressure (kPa) and temperature (°C) blocks with flowing guide dashes, and a blinking BLE link indicator.
- **Navigation changed**: vertical swipe from the home screen no longer opens the weather page (weather is still available via left/right swipe). Page dots now reflect the new up/down pages.

## Note: simulated data

> **Both new pages currently display SIMULATED data and are labeled `SIM` on screen.**
> - The firmware has no IMU, so the G value is simulated (smooth multi-sine wander). A phone-side `#G` channel can replace it later.
> - There is no BLE tire-pressure sensor hardware yet, so pressure/temperature figures are simulated. Real sensors will be fed through the phone app (`#TP` passthrough command) in a future version.
> - Speed uses the real `speedKmh` pushed by the phone (`#v`) when available; otherwise it is simulated so the dial still demonstrates the full range.

## Unchanged

- Everything else from 7.12.9-ui.5 (idle weather on first connect, pixel second-hand ring, `--:--` placeholder, PWR short-press screen off) is unchanged.
