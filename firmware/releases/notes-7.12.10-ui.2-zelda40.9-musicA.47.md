# Firmware 7.12.10-ui.2-zelda40.9-musicA.47

Hotfix on musicA.46. Pair with iOS **0.16.2.41**. Do not merge upstream.

## Stack canary on GTA VI inflate

- **Symptom:** After `PWR timing`, `Stack canary watchpoint triggered (loopTask)` before `MotoNav ready` when NVS `bootbrand=18` (GTA VI). Same bin could boot once on default Zelda then crash after GTA was selected (NVS kept on app-only reflash).
- **Cause:** `BootAnimation::inflateZlibRgb565` put `tinfl_decompressor` (~11 KiB) on `loopTask` stack (Arduino default **8192**). Used by GTA boot id 18 and center vehId 3.
- **Fix:** Static heap `malloc` for the inflater (same pattern as `RasterDecoder` cover/badge/bg). No loop-stack bump required.

## Baseline

- `cursor/gta6-wallpaper-persist-5343` @ `b689f57` (musicA.46)
