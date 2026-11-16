#!/usr/bin/env python3
"""Bake Shenhua boot + center idle/connected frames into PROGMEM zlib headers.

Input (RGBA 466x466, true transparent):
  boot:       .../media/shenhua-anim-mockup-v2/boot/*.png       (AA soft-edge; musicA.82+)
              fallback .../media/shenhua-anim-mockup/boot/      (v1)
  idle/conn:  .../media/shenhua-anim-mockup-v2/{idle,connected}/*.png

Output:
  ShenhuaBootFrames.h   — 466x466 RGB565 opaque (black bg), 1:1 blit like GTA
  ShenhuaCenterIdle.h   — CENTER_W×CENTER_H RGB565 chroma 0
  ShenhuaCenterConn.h   — CENTER_W×CENTER_H RGB565 chroma 0

musicA.80 used half-res 233² + nearest 2× (jagged crest). musicA.82 stores full 466
with soft-alpha composite + 16-color quant (fits 3 MiB after v2 center savings).
musicA.83 rebakes FO-229 idle/connected into 210x150; HomePage draws nearest 2×
(~420x300). musicA.86: ~189×135 (≈90%) + softer alpha key (keep semi-opaque edges,
strip residual H/V guide-box lines).
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

# musicA.86: ~90% of musicA.83/85 210×150 → on-screen 2× ≈ 378×270
# musicA.87: same size; vivid color boost toward user-ref-bright (royal blue / punchy red)
CENTER_W, CENTER_H = 189, 135
CENTER_COLORS = 18  # slightly richer palette for saturated blues/reds
# Soft key: keep semi-opaque fringe (was hard ≥128 → punch-through holes on cloth/body)
ALPHA_KEEP = 24
IDLE_TOTAL_MS = 3500
CONN_TOTAL_MS = 2400
IDLE_COUNT = CONN_COUNT = 8


def quant_rgb(im_rgb: Image.Image, colors: int) -> np.ndarray:
    q = im_rgb.quantize(colors=colors, method=Image.MEDIANCUT, dither=Image.NONE)
    return np.array(q.convert("RGB"))


def boost_shenhua_vivid(rgb: np.ndarray) -> np.ndarray:
    """Match user-ref-bright: lift luma, punch sat, keep shadows chromatic (not muddy gray).

    Source idle/conn frames are much darker than the ref (luma p50 ~16–19 round vs ~146).
    musicA.87: stronger gamma/sat + blue/red lifts vs first pass (still too navy/muddy).
    """
    im = Image.fromarray(rgb, mode="RGB").convert("HSV")
    h, s, v = (np.array(c, dtype=np.float32) for c in im.split())
    # PIL HSV: H 0–255 (~hue*255/360*256 style), S/V 0–255
    # Brighten midtones; leave near-black outlines
    mask = v > 8
    v2 = v.copy()
    s2 = s.copy()
    v2[mask] = np.clip((v[mask] / 255.0) ** 0.52 * 1.22 * 255.0 + 18.0, 0, 255)
    s2[mask] = np.clip(s[mask] * 1.58 + 18.0, 0, 255)
    # Royal blue band in PIL hue (~0.52–0.72 of circle → ~133–184)
    blue = mask & (h >= 125) & (h <= 195) & (s > 28)
    s2[blue] = np.clip(s2[blue] * 1.28 + 28.0, 0, 255)
    v2[blue] = np.clip(v2[blue] * 1.18 + 22.0, 0, 255)
    h2 = h.copy()
    h2[blue] = 148 + (h[blue] - 148) * 0.45  # pull toward ~210° royal
    # Punchy red: PIL hue near 0 / 255
    red = mask & ((h <= 18) | (h >= 235)) & (s > 28)
    s2[red] = np.clip(s2[red] * 1.32 + 32.0, 0, 255)
    v2[red] = np.clip(v2[red] * 1.18 + 18.0, 0, 255)
    near0 = red & (h <= 18)
    near255 = red & (h >= 235)
    h2[near0] = h[near0] * 0.4
    h2[near255] = 250 + (h[near255] - 250) * 0.35
    # Lift near-white / highlight fringe (crisp whites)
    pale = mask & (s < 55) & (v > 140)
    v2[pale] = np.clip(v2[pale] * 1.12 + 20.0, 0, 255)
    s2[pale] = np.clip(s2[pale] * 0.7, 0, 255)
    out = Image.merge(
        "HSV",
        (
            Image.fromarray(np.clip(h2, 0, 255).astype(np.uint8), mode="L"),
            Image.fromarray(np.clip(s2, 0, 255).astype(np.uint8), mode="L"),
            Image.fromarray(np.clip(v2, 0, 255).astype(np.uint8), mode="L"),
        ),
    ).convert("RGB")
    return np.array(out)


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


def strip_guide_box_lines(arr: np.ndarray) -> np.ndarray:
    """Zero residual H/V mid-gray / light guide-box strokes (FO-229 cell borders)."""
    out = arr.copy()
    rgb = out[..., :3].astype(np.int16)
    a = out[..., 3]
    lum = (rgb[..., 0].astype(np.int32) + rgb[..., 1] + rgb[..., 2]) // 3
    # mid-gray ~70–120 or near-white guide; low saturation
    mx = rgb.max(axis=-1)
    mn = rgb.min(axis=-1)
    flat = (mx - mn) <= 18
    guide = (a >= 8) & flat & (
        ((lum >= 70) & (lum <= 130)) | ((lum >= 180) & (lum <= 245))
    )
    h, w = a.shape
    # keep only long H/V runs (≥12 px) so character strokes survive
    kill = np.zeros((h, w), dtype=bool)
    for y in range(h):
        x = 0
        while x < w:
            if not guide[y, x]:
                x += 1
                continue
            x1 = x
            while x1 < w and guide[y, x1]:
                x1 += 1
            if x1 - x >= 12:
                kill[y, x:x1] = True
            x = x1
    for x in range(w):
        y = 0
        while y < h:
            if not guide[y, x]:
                y += 1
                continue
            y1 = y
            while y1 < h and guide[y1, x]:
                y1 += 1
            if y1 - y >= 12:
                kill[y:y1, x] = True
            y = y1
    out[kill, 3] = 0
    return out


def soft_alpha_valid(alpha: np.ndarray) -> np.ndarray:
    """Morphological close + soft threshold so body/cloth edges stay opaque."""
    a = Image.fromarray(alpha, mode="L")
    # close = dilate then erode: fills 1–2 px punch holes, keeps outer silhouette
    a = a.filter(ImageFilter.MaxFilter(5))
    a = a.filter(ImageFilter.MinFilter(3))
    return np.array(a) >= ALPHA_KEEP


def fill_interior_alpha_holes(arr: np.ndarray, passes: int = 2) -> np.ndarray:
    """Fill punch-through holes: transparent pixels ringed by opaque get neighbor RGB + opaque α."""
    out = arr.copy()
    for _ in range(passes):
        a = out[..., 3]
        rgb = out[..., :3].astype(np.uint16)
        high = a >= 180
        low = a < ALPHA_KEEP
        h, w = a.shape
        pad_h = np.pad(high.astype(np.uint8), 1)
        pad_r = np.pad(rgb[..., 0], 1)
        pad_g = np.pad(rgb[..., 1], 1)
        pad_b = np.pad(rgb[..., 2], 1)
        pad_a = np.pad(a, 1)
        neigh = np.zeros((h, w), dtype=np.uint8)
        acc_r = np.zeros((h, w), dtype=np.uint32)
        acc_g = np.zeros((h, w), dtype=np.uint32)
        acc_b = np.zeros((h, w), dtype=np.uint32)
        acc_n = np.zeros((h, w), dtype=np.uint16)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dy == 0 and dx == 0:
                    continue
                sy = slice(1 + dy, 1 + dy + h)
                sx = slice(1 + dx, 1 + dx + w)
                mask = pad_h[sy, sx]
                neigh += mask
                acc_r += pad_r[sy, sx] * mask
                acc_g += pad_g[sy, sx] * mask
                acc_b += pad_b[sy, sx] * mask
                acc_n += mask.astype(np.uint16)
        holes = low & (neigh >= 5) & (acc_n > 0)
        if not holes.any():
            break
        out[holes, 0] = (acc_r[holes] // acc_n[holes]).astype(np.uint8)
        out[holes, 1] = (acc_g[holes] // acc_n[holes]).astype(np.uint8)
        out[holes, 2] = (acc_b[holes] // acc_n[holes]).astype(np.uint8)
        out[holes, 3] = 255
    return out


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
    cleaned = []
    boxes = []
    for im in frames:
        arr = fill_interior_alpha_holes(strip_guide_box_lines(np.array(im)), passes=3)
        cleaned.append(arr)
        ys, xs = np.where(arr[..., 3] >= ALPHA_KEEP)
        boxes.append((int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())))
    x0 = max(0, min(b[0] for b in boxes) - 2)
    y0 = max(0, min(b[1] for b in boxes) - 2)
    x1 = min(465, max(b[2] for b in boxes) + 2)
    y1 = min(465, max(b[3] for b in boxes) + 2)
    payloads = []
    for arr in cleaned:
        crop = Image.fromarray(arr).crop((x0, y0, x1 + 1, y1 + 1))
        cw, ch = crop.size
        s = min(CENTER_W / cw, CENTER_H / ch)
        nw, nh = max(1, round(cw * s)), max(1, round(ch * s))
        # LANCZOS downscale keeps soft edges; NEAREST punched AA into holes
        resized = crop.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new("RGBA", (CENTER_W, CENTER_H), (0, 0, 0, 0))
        ox, oy = (CENTER_W - nw) // 2, (CENTER_H - nh) // 2
        canvas.paste(resized, (ox, oy), resized)
        arr2 = fill_interior_alpha_holes(np.array(canvas), passes=2)
        valid = soft_alpha_valid(arr2[..., 3])
        vivid = boost_shenhua_vivid(arr2[..., :3])
        rgb = quant_rgb(Image.fromarray(vivid), CENTER_COLORS)
        payloads.append(pack565(rgb, valid))
    emit_header(OUT / out_name, prefix, CENTER_W, CENTER_H, count, total_ms,
                payloads, f"shenhua-{sub}-v2-soft90-vivid")


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
