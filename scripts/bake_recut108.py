#!/usr/bin/env python3
"""musicA.108: bake the RECUT animations (true frame boundaries, +4px hand
clearance, idle y24 compact / conn floor-tile removed) into firmware headers.

Pipeline = _anim_recut.py geometry  +  bake_new_center_frames.py quantization:
  - recut sheets at true (non-equal) frame boundaries, shifted +4px
  - idle: bottom-aligned unified canvas, crop y>=24, scale to fit 189x135
  - conn: sat<=12 mid-grey floor tiles removed, bottom-aligned, scale to fit
  - premultiplied-alpha LANCZOS resize
  - 32-color MedianCut palette on OPAQUE pixels only (shared across 8 frames)
  - nearest-palette map -> RGB565, chroma 0 = transparent
Output headers are byte-compatible with the existing SHENHUA_{IDLE,CONN} format.
"""
from __future__ import annotations
import sys
from pathlib import Path
import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import bake_new_center_frames as B  # reuse palette / pack565 / emit_header

SRC = Path(r"D:\workbuddy\2026-10-26-08-12-36\resources\MotoNavAnim_Transparent")
PREV = HERE.parent / "_bake_preview"
PREV.mkdir(exist_ok=True)
CW, CH = B.CW, B.CH           # 189, 135
FRAMES = B.FRAMES             # 8

BOUNDS = {
    "set1_fight": [0, 232, 457, 677, 892, 1119, 1338, 1554, 1761],
    "set2_moto": [0, 195, 423, 650, 874, 1100, 1324, 1544, 1752],
}


def recut(name):
    sh = Image.open(SRC / name / "sheet.png").convert("RGBA")
    b = BOUNDS[name]
    out = []
    for k in range(FRAMES):
        f = sh.crop((b[k], 0, b[k + 1], sh.height))
        a = np.array(f)
        col = (a[..., 3] > 24).sum(0)
        nz = np.where(col > 1)[0]
        if len(nz):
            f = f.crop((int(nz[0]), 0, int(nz[-1]) + 1, sh.height))
        out.append(np.array(f))
    return out


def union_bbox(frames):
    x0 = min(int(np.where(f[..., 3] > 16)[1].min()) for f in frames)
    x1 = max(int(np.where(f[..., 3] > 16)[1].max()) for f in frames)
    y0 = min(int(np.where(f[..., 3] > 16)[0].min()) for f in frames)
    y1 = max(int(np.where(f[..., 3] > 16)[0].max()) for f in frames)
    return x0, y0, x1, y1


def floor_mask(a):
    rgb = a[..., :3].astype(np.int16)
    mn, mx = rgb.min(2), rgb.max(2)
    sat = mx - mn
    h = a.shape[0]
    yy = np.arange(h)[:, None]
    lower = yy >= int(h * 0.60)
    midgrey = (mn >= 105) & (mx <= 215)
    return lower & (sat <= 12) & midgrey & (a[..., 3] > 100)


def unified(frames, y_from, clean):
    """Bottom-aligned shared canvas -> crop -> premult resize -> 189x135 RGBA list."""
    canv = []
    maxh = max(f.shape[0] for f in frames)
    maxw = max(f.shape[1] for f in frames)
    for f in frames:
        g = f.copy()
        if clean:
            g[..., 3][floor_mask(g)] = 0
        c = np.zeros((maxh, maxw, 4), np.uint8)
        c[maxh - g.shape[0]:, :g.shape[1]] = g
        canv.append(c)
    x0, _, x1, y1 = union_bbox(canv)
    bw, bh = x1 - x0 + 1, y1 - y_from + 1
    s = min(CW / bw, CH / bh)
    nw, nh = round(bw * s), round(bh * s)
    ox, oy = (CW - nw) // 2, (CH - nh) // 2
    out = []
    for c in canv:
        cc = np.zeros((CH, CW, 4), np.uint8)
        cc[oy:oy + nh, ox:ox + nw] = B.resize_rgba_straight(
            Image.fromarray(c[y_from:y1 + 1, x0:x1 + 1]), nw, nh)
        out.append(cc)
    print(f"  unified: y_from={y_from} bbox x{x0}-{x1} y{y_from}-{y1} "
          f"scale {s:.3f} -> {nw}x{nh} at ({ox},{oy})")
    return out


def bake(cfg, frames_rgba, colors=32):
    # opaque-only shared palette
    rgb_list, valids = [], []
    for im in frames_rgba:
        rgb_list.append(im[..., :3].copy())
        valids.append(im[..., 3] >= 128)
    pal = B.shared_palette(rgb_list, valids, colors)
    payloads, previews = [], []
    for k in range(FRAMES):
        qrgb = B.apply_palette(rgb_list[k], pal)
        raw = np.frombuffer(B.pack565(qrgb, valids[k]), "<u2").reshape(CH, CW)
        payloads.append(raw.astype("<u2").tobytes())
        rr = (((raw >> 11) & 0x1F).astype(np.uint32) * 255 // 31).astype(np.uint8)
        gg = (((raw >> 5) & 0x3F).astype(np.uint32) * 255 // 63).astype(np.uint8)
        bb = (((raw & 0x1F).astype(np.uint32)) * 255 // 31).astype(np.uint8)
        previews.append(np.dstack([rr, gg, bb, np.where(raw > 0, 255, 0).astype(np.uint8)]))
    B.emit_header(cfg["out"], cfg["prefix"], CW, CH, FRAMES,
                  cfg["total_ms"], payloads, "musicA.108 recut")
    # preview on magenta so transparency is obvious
    mag = np.array([255, 0, 255], np.uint8)
    tiles = []
    for rgba in previews:
        al = rgba[..., 3].astype(float) / 255
        tiles.append((rgba[..., :3] * al[..., None] + mag * (1 - al[..., None])).astype(np.uint8))
    grid = np.concatenate([np.concatenate(tiles[r * 4:(r + 1) * 4], axis=1) for r in range(2)], axis=0)
    Image.fromarray(grid).resize((grid.shape[1] * 2, grid.shape[0] * 2), Image.NEAREST).save(
        PREV / (cfg["tag"] + "_108.png"))
    print(f"  preview -> {PREV / (cfg['tag'] + '_108.png')}")


def main():
    OUT = HERE.parent / "firmware" / "boards" / "175" / "MotoNav" / "resources"
    cfgs = {
        "idle": dict(dir="set1_fight", out=OUT / "ShenhuaCenterIdle.h",
                     prefix="SHENHUA_IDLE", total_ms=3500, tag="idle_565"),
        "conn": dict(dir="set2_moto", out=OUT / "ShenhuaCenterConn.h",
                     prefix="SHENHUA_CONN", total_ms=2400, tag="conn_565"),
    }
    for key in ("idle", "conn"):
        cfg = cfgs[key]
        print(f"=== {key} ===")
        frames = recut(cfg["dir"])
        rgba = unified(frames, y_from=24 if key == "idle" else 0,
                       clean=(key == "conn"))
        bake(cfg, rgba, colors=32)


if __name__ == "__main__":
    main()
