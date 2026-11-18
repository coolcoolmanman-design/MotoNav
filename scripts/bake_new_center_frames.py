#!/usr/bin/env python3
"""Bake the two NEW 8-frame transparent animations into firmware center-frame headers.

Reads the cleaned per-frame transparent PNGs produced from the user's two ref sheets:
  set1_fight  -> ShenhuaCenterIdle.h   (待机/待连接态)
  set2_moto   -> ShenhuaCenterConn.h   (连接态)

Output format is byte-compatible with the existing headers:
  <PREFIX>_W/H = 189/135, _COUNT 8, _TOTAL_MS/_FRAME_MS unchanged,
  zlib(RGB565, level 9), chroma 0 = transparent, foreground color never 0.
  Header files are written CRLF (git autocrlf convention).
"""
from __future__ import annotations
import argparse, zlib
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = Path(r"D:\workbuddy\2026-10-26-08-12-36\resources\MotoNavAnim_Transparent")
OUT = ROOT / "firmware" / "boards" / "175" / "MotoNav" / "resources"

FRAMES = 8
CW, CH = 189, 135
SETS = {
    "idle": dict(dir="set1_fight", out="ShenhuaCenterIdle.h", prefix="SHENHUA_IDLE",
                 total_ms=3500),
    "conn": dict(dir="set2_moto", out="ShenhuaCenterConn.h", prefix="SHENHUA_CONN",
                 total_ms=2400),
}


def quant_rgb(im_rgb: Image.Image, colors: int) -> np.ndarray:
    q = im_rgb.quantize(colors=colors, method=Image.MEDIANCUT, dither=Image.NONE)
    return np.array(q.convert("RGB"))


def pack565(rgb: np.ndarray, valid: np.ndarray) -> bytes:
    r = rgb[..., 0].astype(np.uint16) >> 3
    g = rgb[..., 1].astype(np.uint16) >> 2
    b = rgb[..., 2].astype(np.uint16) >> 3
    c = ((r << 11) | (g << 5) | b).astype(np.uint16)
    c[~valid] = 0
    c[valid & (c == 0)] = 1  # never let a real foreground pixel become chroma 0
    return c.astype("<u2").tobytes()


def emit_header(path: Path, prefix: str, w: int, h: int, count: int,
                total_ms: int, payloads: list[bytes], note: str) -> None:
    zlist = [zlib.compress(p, 9) for p in payloads]
    zlens = [len(z) for z in zlist]
    zoffs, off = [], 0
    for n in zlens:
        zoffs.append(off); off += n
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
            lines.append("  " + ",".join(row) + ","); row = []
    if row:
        lines.append("  " + ",".join(row) + ",")
    lines.append("};")
    path.write_bytes(("\n".join(lines) + "\n").replace("\n", "\r\n").encode("ascii"))
    print(f"wrote {path.name}: frames={count} zlib={len(blob)} B zlens={zlens}")


def bake(key: str, colors: int, preview: bool) -> None:
    cfg = SETS[key]
    fd = SRC / cfg["dir"] / "frames"
    ims = []
    for k in range(FRAMES):
        p = fd / f"{cfg['dir']}_{k:02d}_u.png"  # unified canvas (same origin across frames)
        ims.append(np.array(Image.open(p).convert("RGBA")))
    # union bbox across all frames (alpha>0)
    x0 = min(int(np.where(im[..., 3] > 0)[1].min()) for im in ims)
    x1 = max(int(np.where(im[..., 3] > 0)[1].max()) for im in ims)
    y0 = min(int(np.where(im[..., 3] > 0)[0].min()) for im in ims)
    y1 = max(int(np.where(im[..., 3] > 0)[0].max()) for im in ims)
    print(f"{key}: union bbox x{x0}-{x1} y{y0}-{y1} ({(x1-x0+1)}x{(y1-y0+1)})")
    s = min(CW / (x1 - x0 + 1), CH / (y1 - y0 + 1))
    nw, nh = max(1, round((x1 - x0 + 1) * s)), max(1, round((y1 - y0 + 1) * s))
    ox, oy = (CW - nw) // 2, (CH - nh) // 2
    print(f"  scale {s:.3f} -> {nw}x{nh} at ({ox},{oy})")

    payloads = []
    previews = []
    for k, im in enumerate(ims):
        crop = Image.fromarray(im[y0:y1 + 1, x0:x1 + 1])
        resized = crop.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
        canvas.paste(resized, (ox, oy), resized)
        a = np.array(canvas)
        alpha = a[..., 3]
        valid = alpha >= 128
        qrgb = quant_rgb(Image.fromarray(a[..., :3]), colors)
        # preview: decode back from 565 to see exactly what firmware shows
        raw = np.frombuffer(pack565(qrgb, valid), "<u2").reshape(CH, CW)
        rr = (((raw >> 11) & 0x1F).astype(np.uint32) * 255 // 31).astype(np.uint8)
        gg = (((raw >> 5) & 0x3F).astype(np.uint32) * 255 // 63).astype(np.uint8)
        bb = (((raw & 0x1F).astype(np.uint32)) * 255 // 31).astype(np.uint8)
        rgba = np.dstack([rr, gg, bb, np.where(raw > 0, 255, 0).astype(np.uint8)])
        previews.append(rgba)
        payloads.append(raw.astype("<u2").tobytes())

    if preview:
        # magenta background so any leftover white line is obvious
        mag = np.array([255, 0, 255], np.uint8)
        tiles = []
        for rgba in previews:
            al = rgba[..., 3].astype(float) / 255
            tiles.append((rgba[..., :3] * al[..., None] + mag * (1 - al[..., None])).astype(np.uint8))
        rows = [np.concatenate(tiles[r * 4:(r + 1) * 4], axis=1) for r in range(2)]
        grid = np.concatenate(rows, axis=0)
        prev_dir = ROOT / "_bake_preview"; prev_dir.mkdir(exist_ok=True)
        Image.fromarray(grid).resize((grid.shape[1] * 2, grid.shape[0] * 2), Image.NEAREST).save(
            prev_dir / f"{key}_565.png")
        print(f"  preview -> {prev_dir / (key + '_565.png')}")
    else:
        emit_header(OUT / cfg["out"], cfg["prefix"], CW, CH, FRAMES,
                    cfg["total_ms"], payloads, f"{key} new animation")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--colors", type=int, default=16)
    ap.add_argument("--preview", action="store_true")
    ap.add_argument("--set", choices=["idle", "conn", "both"], default="both")
    args = ap.parse_args()
    keys = ["idle", "conn"] if args.set == "both" else [args.set]
    for k in keys:
        bake(k, args.colors, args.preview)
