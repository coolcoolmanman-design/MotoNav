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


def resize_rgba_straight(crop: Image.Image, nw: int, nh: int) -> np.ndarray:
    """Premultiplied-alpha resize then un-premultiply: avoids dark/gray halo
    from LANCZOS blending straight RGB with the transparent canvas."""
    a = np.array(crop).astype(np.float32)
    alpha = a[..., 3:4] / 255.0
    prem = np.dstack([a[..., 0] * alpha[..., 0], a[..., 1] * alpha[..., 0], a[..., 2] * alpha[..., 0],
                      a[..., 3]])
    img = Image.fromarray(np.clip(prem, 0, 255).astype(np.uint8), "RGBA")
    img = img.resize((nw, nh), Image.LANCZOS)
    r = np.array(img).astype(np.float32)
    al = r[..., 3:4] / 255.0
    keep = al[..., 0] > 0.004
    rgb = np.zeros_like(r[..., :3])
    rgb[keep] = r[..., :3][keep] / al[..., 0][keep][:, None]
    rgb = np.clip(rgb, 0, 255)
    return np.dstack([rgb.astype(np.uint8), r[..., 3].astype(np.uint8)])


def boost_sat(rgb: np.ndarray, factor: float) -> np.ndarray:
    """Multiplicative saturation boost around luma (compensates palette/565 wash-out)."""
    if factor == 1.0:
        return rgb
    f = rgb.astype(np.float32)
    lum = (f[..., 0] * 0.299 + f[..., 1] * 0.587 + f[..., 2] * 0.114)[..., None]
    out = lum + (f - lum) * factor
    return np.clip(out, 0, 255).astype(np.uint8)


def shared_palette(frames_rgb: list[np.ndarray], valids: list[np.ndarray], colors: int):
    """MedianCut palette computed on OPAQUE pixels only, shared across all frames."""
    pix = np.concatenate([f[v] for f, v in zip(frames_rgb, valids)])
    if len(pix) > 400000:  # subsample for quantize speed, keep distribution
        idx = np.random.default_rng(7).choice(len(pix), 400000, replace=False)
        pix_q = pix[idx]
    else:
        pix_q = pix
    img = Image.fromarray(pix_q.reshape(-1, 1, 3))
    q = img.quantize(colors=colors, method=Image.MEDIANCUT, dither=Image.NONE)
    pal = np.array(q.getpalette()[: colors * 3], dtype=np.float32).reshape(colors, 3)
    return pal


def apply_palette(rgb: np.ndarray, pal: np.ndarray) -> np.ndarray:
    """Nearest-palette mapping, vectorized over unique colors."""
    flat = rgb.reshape(-1, 3).astype(np.int64)
    key = (flat[:, 0] << 16) + (flat[:, 1] << 8) + flat[:, 2]
    uk, inv = np.unique(key, return_inverse=True)
    uc = np.dstack([uk >> 16, (uk >> 8) & 0xFF, uk & 0xFF])[0].astype(np.float32)
    # nearest palette entry (chunked to bound memory)
    nearest = np.empty(len(uc), dtype=np.uint8)
    pal32 = pal.astype(np.float32)
    for s in range(0, len(uc), 8192):
        chunk = uc[s:s + 8192]
        d = ((chunk[:, None, :] - pal32[None, :, :]) ** 2).sum(-1)
        nearest[s:s + 8192] = d.argmin(1)
    out = pal[nearest[inv]].astype(np.uint8)
    return out.reshape(rgb.shape)


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


def bake(key: str, colors: int, preview: bool, sat: float = 1.0) -> None:
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

    # pass 1: resize (premultiplied) + saturation boost -> per-frame RGB/valid
    frames_rgb, valids, canvases = [], [], []
    for k, im in enumerate(ims):
        crop = Image.fromarray(im[y0:y1 + 1, x0:x1 + 1])
        rs = resize_rgba_straight(crop, nw, nh)
        canvas = np.zeros((CH, CW, 4), np.uint8)
        canvas[oy:oy + nh, ox:ox + nw] = rs
        rgb = boost_sat(canvas[..., :3].copy(), sat)
        frames_rgb.append(rgb)
        valids.append(canvas[..., 3] >= 128)
        canvases.append(canvas)

    # pass 2: shared palette on opaque pixels only, then nearest mapping
    pal = shared_palette(frames_rgb, valids, colors)
    print(f"  shared palette {colors} colors (opaque-only), sat={sat}")

    payloads = []
    previews = []
    for k in range(FRAMES):
        qrgb = apply_palette(frames_rgb[k], pal)
        raw = np.frombuffer(pack565(qrgb, valids[k]), "<u2").reshape(CH, CW)
        payloads.append(raw.astype("<u2").tobytes())
        # preview: decode back from 565 to see exactly what firmware shows
        rr = (((raw >> 11) & 0x1F).astype(np.uint32) * 255 // 31).astype(np.uint8)
        gg = (((raw >> 5) & 0x3F).astype(np.uint32) * 255 // 63).astype(np.uint8)
        bb = (((raw & 0x1F).astype(np.uint32)) * 255 // 31).astype(np.uint8)
        rgba = np.dstack([rr, gg, bb, np.where(raw > 0, 255, 0).astype(np.uint8)])
        previews.append(rgba)

    if preview:
        mag = np.array([255, 0, 255], np.uint8)
        tiles = []
        for rgba in previews:
            al = rgba[..., 3].astype(float) / 255
            tiles.append((rgba[..., :3] * al[..., None] + mag * (1 - al[..., None])).astype(np.uint8))
        rows = [np.concatenate(tiles[r * 4:(r + 1) * 4], axis=1) for r in range(2)]
        grid = np.concatenate(rows, axis=0)
        prev_dir = ROOT / "_bake_preview"; prev_dir.mkdir(exist_ok=True)
        tag = f"{key}_565" if sat == 1.0 else f"{key}_565_sat{sat}"
        Image.fromarray(grid).resize((grid.shape[1] * 2, grid.shape[0] * 2), Image.NEAREST).save(
            prev_dir / f"{tag}.png")
        print(f"  preview -> {prev_dir / (tag + '.png')}")
    else:
        emit_header(OUT / cfg["out"], cfg["prefix"], CW, CH, FRAMES,
                    cfg["total_ms"], payloads, f"{key} new animation")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--colors", type=int, default=32)
    ap.add_argument("--sat", type=float, default=1.0)
    ap.add_argument("--preview", action="store_true")
    ap.add_argument("--set", choices=["idle", "conn", "both"], default="both")
    args = ap.parse_args()
    keys = ["idle", "conn"] if args.set == "both" else [args.set]
    for k in keys:
        bake(k, args.colors, args.preview, args.sat)
