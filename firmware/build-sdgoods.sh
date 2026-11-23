#!/usr/bin/env bash
# Build MotoNav for the sdgoods (谷仓) board.
# MULTI deliverable = pure app.bin (pack_app validated). FULL image is SINGLE-only.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BOARD=sdgoods
PANEL_VARIANT="${PANEL_VARIANT:-0}"
CLI="${ARDUINO_CLI:-arduino-cli}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
FQBN='esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc'
PACK_APP="${PACK_APP:-/agent/repos/sdgoods-esp32s3/tools/pack_app.py}"

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

export SDG_OUT="$OUT" SDG_DEST="$DEST" SDG_NAME="$NAME" SDG_PACK_APP="$PACK_APP"
python3 - <<'PY'
from pathlib import Path
import hashlib, json, os, shutil, struct, subprocess, sys

out = Path(os.environ["SDG_OUT"])
dest = Path(os.environ["SDG_DEST"])
name = os.environ["SDG_NAME"]
app = (out / "MotoNav.ino.bin").read_bytes()
merged = (out / "MotoNav.ino.merged.bin").read_bytes()
need = 0x10000 + len(app)
if len(merged) < need:
    raise SystemExit(f"merged image too short: {len(merged)} < {need}")
seg = merged[0x10000:need]
if seg != app:
    raise SystemExit("unexpected application offset in merged image")

magic = struct.unpack_from("<I", app, 0x20)[0]
if magic != 0xABCD5432:
    raise SystemExit(f"bad esp_app_desc magic: {magic:#x}")
project = app[0x50:0x70].split(b"\0", 1)[0].decode("utf-8", "replace")
version = app[0x30:0x50].split(b"\0", 1)[0].decode("utf-8", "replace")
print(f"esp_app_desc project_name={project!r} version={version!r} size={len(app)}")
if project != "MotoNav":
    raise SystemExit(f"app_id must be MotoNav, got {project!r} (EspAppDesc override failed?)")

app_path = dest / f"{name}_app_0x10000.bin"
full_path = dest / f"{name}_full_0x0.bin"
multi_path = dest / "MotoNav_app.bin"  # MULTI primary deliverable (no flash address)
app_path.write_bytes(app)
full_path.write_bytes(merged[:need])
shutil.copyfile(app_path, multi_path)

meta = {
    "app_id": project,
    "version": version,
    "sizeBytes": len(app),
    "sha256": hashlib.sha256(app).hexdigest(),
    "deliverables": {
        "MULTI": multi_path.name,
        "SINGLE_full_overwrite": full_path.name,
        "app_alias": app_path.name,
    },
    "note": "MULTI: install MotoNav_app.bin via launcher/platform only. Do NOT flash *_full_0x0.bin on MULTI devices.",
}
(dest / "MotoNav_app.meta.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
print("wrote", app_path, len(app), "bytes")
print("wrote", full_path, need, "bytes")
print("wrote", multi_path, "(MULTI primary)")

pack = Path(os.environ.get("SDG_PACK_APP", ""))
if pack.is_file():
    cmd = [sys.executable, str(pack), "-i", str(multi_path), "--no-emit", "--json"]
    print("pack_app:", " ".join(cmd))
    subprocess.check_call(cmd)
else:
    print("WARN: pack_app.py not found at", pack, "- skipped format gate")
PY
ls -la "$DEST"
