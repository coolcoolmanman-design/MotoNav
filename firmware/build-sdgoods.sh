#!/usr/bin/env bash
# Build MotoNav for the sdgoods (谷仓) board. MVP gate = compile success.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BOARD=sdgoods
PANEL_VARIANT="${PANEL_VARIANT:-0}"
CLI="${ARDUINO_CLI:-arduino-cli}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
FQBN='esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc'

BUILD_ROOT="$ROOT/.build/$BOARD"
SKETCH="$BUILD_ROOT/MotoNav"
OUT="$BUILD_ROOT/output"
SRC="$ROOT/boards/$BOARD/MotoNav"
mkdir -p "$SKETCH" "$OUT"

# Stage sketch sources (ino/h/c/cpp only, mirror build.ps1).
while IFS= read -r -d '' f; do
  rel="${f#"$SRC"/}"
  dest="$SKETCH/$rel"
  mkdir -p "$(dirname "$dest")"
  cp -f "$f" "$dest"
done < <(find "$SRC" -type f \( -name '*.ino' -o -name '*.h' -o -name '*.cpp' -o -name '*.c' \) -print0)

# Optional panel init variant override (0=auto, 1/2=fixed).
sed -i "s/#define LCD_INIT_VARIANT [0-9][0-9]*/#define LCD_INIT_VARIANT $PANEL_VARIANT/" "$SKETCH/pin_config.h"

PREFIX_MAP="-ffile-prefix-map=$HOME=/builder"
"$CLI" compile \
  --jobs "$JOBS" \
  --libraries "$ROOT/libraries" \
  --build-path "$BUILD_ROOT/build" \
  --fqbn "$FQBN" \
  --build-property "compiler.c.extra_flags=$PREFIX_MAP" \
  --build-property "compiler.cpp.extra_flags=$PREFIX_MAP" \
  --output-dir "$OUT" \
  "$SKETCH"

VERSION="$(grep -E '^#define FW_VERSION "v' "$SKETCH/MotoNav.ino" | head -1 | sed -E 's/.*"v([^"]+)".*/\1/')"
DEST="$ROOT/dist/$BOARD/panel$PANEL_VARIANT"
mkdir -p "$DEST"
NAME="MotoNav_universal_sdgoods_v${VERSION}_panel${PANEL_VARIANT}"
cp -f "$OUT/MotoNav.ino.bin" "$DEST/${NAME}_app_0x10000.bin"
# Trim merged image to bootloader+partitions+app (same approach as build.ps1).
python3 - <<PY
from pathlib import Path
out = Path("$OUT")
dest = Path("$DEST")
app = (out / "MotoNav.ino.bin").read_bytes()
merged = (out / "MotoNav.ino.merged.bin").read_bytes()
need = 0x10000 + len(app)
if len(merged) < need:
    raise SystemExit(f"merged image too short: {len(merged)} < {need}")
seg = merged[0x10000:need]
if seg != app:
    raise SystemExit("unexpected application offset in merged image")
(dest / "${NAME}_full_0x0.bin").write_bytes(merged[:need])
print("wrote", dest / "${NAME}_app_0x10000.bin", len(app), "bytes")
print("wrote", dest / "${NAME}_full_0x0.bin", need, "bytes")
PY
ls -la "$DEST"
