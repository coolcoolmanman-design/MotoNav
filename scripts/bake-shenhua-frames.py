#!/usr/bin/env python3
"""Bake Shenhua boot + center idle/connected frames into PROGMEM zlib headers.

Input (RGBA 466x466, true transparent):
  boot:       .../media/shenhua-anim-mockup-v2/boot/*.png       (AA soft-edge; musicA.82+)
              fallback .../media/shenhua-anim-mockup/boot/      (v1)
  idle/conn:  .../media/shenhua-anim-mockup-v2/{idle,connected}/*.png

Output:
  ShenhuaBootFrames.h   — 466x466 RGB565 opaque (black bg), 1:1 blit like GTA
  ShenhuaCenterIdle.h   — 210x150 RGB565 chroma 0
  ShenhuaCenterConn.h   — 210x150 RGB565 chroma 0

musicA.80 used half-res 233² + nearest 2× (jagged crest). musicA.82 stores full 466
with soft-alpha composite + 16-color quant (fits 3 MiB after v2 center savings).
"""
from __future__ import annotations

import argparse
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
STORE_V1 = Path("/cursor/stores/bc-01a18cf8-eab5-782d-9528-f6ca07999af5/media/shenhua-anim-mockup")
STORE_V2 = Path("/cursor/stores/bc-01a18cf8-eab5-782d-9528-f6ca07999af5/media/shenhua-anim-mockup-v2")
OUT = ROOT / "firmware" / "boards" / "175" / "MotoNav" / "resources"

BOOT_W = BOOT_H = 466
BOOT_COLORS = 14   # musicA.82: full 466 AA; 14 keeps ~5 KiB app-slot margin
BOOT_TOTAL_MS = 2700
BOOT_COUNT = 9
BOOT_FEATHER = 0.8

CENTER_W, CENTER_H = 210, 150
CENTER_COLORS = 16
IDLE_TOTAL_MS = 3500
CONN_TOTAL_MS = 2400
IDLE_COUNT = CONN_COUNT = 8


def quant_rgb(im_rgb: Image.Image, colors: int) -> np.ndarray:
    q = im_rgb.quantize(colors=colors, method=Image.MEDIANCUT, dither=Image.NONE)
    return np.array(q.convert("RGB"))


def pack565(rgb: np.ndarray, valid: np.ndarray | None = None) -> bytes:
    r = rgb[..., 0].astype(np.uint16) >> 3
    g = rgb[..., 1].astype(np.uint16) >> 2
    b = rgb[..., 2].astype(np.uint16) >> 3
    c = ((r << 11) | (g << 5) | b).astype(np.uint16)
    if valid is not None:
        c[~valid] = 0
        c[valid & (c == 0)] = 1
    return c.astype("<u2").tobytes()


def emit_header(path: Path, prefix: str, w: int, h: int, count: int,
                total_ms: int, payloads: list[bytes], note: str) -> None:
    zlist = [zlib.compress(p, 9) for p in payloads]
    zlens = [len(z) for z in zlist]
    zoffs = []
    off = 0
    for n in zlens:
        zoffs.append(off)
        off += n
    blob = b"".join(zlist)
    frame_ms = total_ms // count
    lines = [
        f"// Auto-generated zlib RGB565 frames for {note}. Do not edit.",
        "#pragma once",
        f"#define {prefix}_W {w}",
        f"#define {prefix}_H {h}",
        f"#define {prefix}_COUNT {count}",
        f"#define {prefix}_TOTAL_MS {total_ms}u",
        f"#define {prefix}_FRAME_MS {frame_ms}u",
        f"static const uint32_t {prefix}_ZLEN[{count}] = {{{','.join(str(n) for n in zlens)}}};",
        f"static const uint32_t {prefix}_ZOFF[{count}] = {{{','.join(str(n) for n in zoffs)}}};",
        f"#define {prefix}_ZTOTAL {len(blob)}",
        f"static const uint8_t {prefix}_ZDATA[] PROGMEM = {{",
    ]
    row = []
    for i, b in enumerate(blob):
        row.append(f"0x{b:02X}")
        if len(row) == 16:
            lines.append("  " + ",".join(row) + ",")
            row = []
    if row:
        lines.append("  " + ",".join(row) + ",")
    lines.append("};")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {path.name}: frames={count} zlib={len(blob)} B")


def soft_black_composite(rgba: Image.Image, feather: float = BOOT_FEATHER) -> Image.Image:
    """Composite onto black with a soft alpha fringe (anti-aliased crest rim)."""
    r, g, b, a = rgba.split()
    if feather > 0:
        a = a.filter(ImageFilter.GaussianBlur(radius=feather))
    soft = Image.merge("RGBA", (r, g, b, a))
    bg = Image.new("RGBA", soft.size, (0, 0, 0, 255))
    return Image.alpha_composite(bg, soft).convert("RGB")


def bake_boot() -> None:
    boot_dir = STORE_V2 / "boot"
    if not boot_dir.is_dir() or not (boot_dir / "01.png").is_file():
        boot_dir = STORE_V1 / "boot"
    payloads = []
    for i in range(1, BOOT_COUNT + 1):
        im = Image.open(boot_dir / f"{i:02d}.png").convert("RGBA")
        if im.size != (BOOT_W, BOOT_H):
            im = im.resize((BOOT_W, BOOT_H), Image.LANCZOS)
        rgb = quant_rgb(soft_black_composite(im), BOOT_COLORS)
        payloads.append(pack565(rgb, valid=None))
    emit_header(OUT / "ShenhuaBootFrames.h", "SHENHUA_BOOT", BOOT_W, BOOT_H,
                BOOT_COUNT, BOOT_TOTAL_MS, payloads, "shenhua-boot-aa-466")


def bake_center(sub: str, prefix: str, count: int, total_ms: int, out_name: str,
                store: Path = STORE_V2) -> None:
    frames = [Image.open(store / sub / f"{i:02d}.png").convert("RGBA") for i in range(1, count + 1)]
    boxes = []
    for im in frames:
        a = np.array(im)
        ys, xs = np.where(a[..., 3] >= 128)
        boxes.append((int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())))
    x0 = max(0, min(b[0] for b in boxes) - 2)
    y0 = max(0, min(b[1] for b in boxes) - 2)
    x1 = min(465, max(b[2] for b in boxes) + 2)
    y1 = min(465, max(b[3] for b in boxes) + 2)
    payloads = []
    for im in frames:
        crop = im.crop((x0, y0, x1 + 1, y1 + 1))
        cw, ch = crop.size
        s = min(CENTER_W / cw, CENTER_H / ch)
        nw, nh = max(1, round(cw * s)), max(1, round(ch * s))
        resized = crop.resize((nw, nh), Image.NEAREST)
        canvas = Image.new("RGBA", (CENTER_W, CENTER_H), (0, 0, 0, 0))
        ox, oy = (CENTER_W - nw) // 2, (CENTER_H - nh) // 2
        canvas.paste(resized, (ox, oy), resized)
        arr = np.array(canvas)
        rgb = quant_rgb(Image.fromarray(arr[..., :3]), CENTER_COLORS)
        payloads.append(pack565(rgb, arr[..., 3] >= 128))
    emit_header(OUT / out_name, prefix, CENTER_W, CENTER_H, count, total_ms,
                payloads, f"shenhua-{sub}-v2")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--center-only", action="store_true",
                    help="Only rebake idle/connected; leave boot header alone")
    ap.add_argument("--boot-only", action="store_true",
                    help="Only rebake boot AA frames; leave center headers alone")
    args = ap.parse_args()
    if not STORE_V2.is_dir():
        raise SystemExit(f"missing v2 mockup dir: {STORE_V2}")
    OUT.mkdir(parents=True, exist_ok=True)
    if args.boot_only:
        bake_boot()
        return
    if not args.center_only:
        bake_boot()
    bake_center("idle", "SHENHUA_IDLE", IDLE_COUNT, IDLE_TOTAL_MS, "ShenhuaCenterIdle.h")
    bake_center("connected", "SHENHUA_CONN", CONN_COUNT, CONN_TOTAL_MS, "ShenhuaCenterConn.h")


if __name__ == "__main__":
    main()
