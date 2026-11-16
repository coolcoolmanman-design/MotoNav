#!/usr/bin/env python3
"""Bake Shenhua boot + center idle/connected frames into PROGMEM zlib headers.

Input (RGBA 466x466, true transparent):
  /cursor/stores/.../media/shenhua-anim-mockup/{boot,idle,connected}/*.png

Output (same pipeline style as GTA/combo):
  firmware/boards/175/MotoNav/resources/ShenhuaBootFrames.h   — 233x233 RGB565
  firmware/boards/175/MotoNav/resources/ShenhuaCenterIdle.h   — 210x150 RGB565 chroma 0
  firmware/boards/175/MotoNav/resources/ShenhuaCenterConn.h   — 210x150 RGB565 chroma 0

Flash budget: app slot is 3 MiB; musicA.79 app ≈ 2.95 MiB. Full 466 frames do not fit.
Boot is half-res (2x nearest at draw); center is cropped 16-color canvas like ComboBuiltin.
"""
from __future__ import annotations

import zlib
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
STORE = Path("/cursor/stores/bc-01a18cf8-eab5-782d-9528-f6ca07999af5/media/shenhua-anim-mockup")
OUT = ROOT / "firmware" / "boards" / "175" / "MotoNav" / "resources"

BOOT_W = BOOT_H = 233
BOOT_COLORS = 16
BOOT_TOTAL_MS = 2700
BOOT_COUNT = 9

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


def bake_boot() -> None:
    payloads = []
    for i in range(1, BOOT_COUNT + 1):
        im = Image.open(STORE / "boot" / f"{i:02d}.png").convert("RGBA")
        bg = Image.new("RGBA", im.size, (0, 0, 0, 255))
        comp = Image.alpha_composite(bg, im).convert("RGB")
        half = comp.resize((BOOT_W, BOOT_H), Image.NEAREST)
        rgb = quant_rgb(half, BOOT_COLORS)
        payloads.append(pack565(rgb, valid=None))
    emit_header(OUT / "ShenhuaBootFrames.h", "SHENHUA_BOOT", BOOT_W, BOOT_H,
                BOOT_COUNT, BOOT_TOTAL_MS, payloads, "shenhua-boot")


def bake_center(sub: str, prefix: str, count: int, total_ms: int, out_name: str) -> None:
    frames = [Image.open(STORE / sub / f"{i:02d}.png").convert("RGBA") for i in range(1, count + 1)]
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
                payloads, f"shenhua-{sub}")


def main() -> None:
    if not STORE.is_dir():
        raise SystemExit(f"missing mockup dir: {STORE}")
    OUT.mkdir(parents=True, exist_ok=True)
    bake_boot()
    bake_center("idle", "SHENHUA_IDLE", IDLE_COUNT, IDLE_TOTAL_MS, "ShenhuaCenterIdle.h")
    bake_center("connected", "SHENHUA_CONN", CONN_COUNT, CONN_TOTAL_MS, "ShenhuaCenterConn.h")


if __name__ == "__main__":
    main()
