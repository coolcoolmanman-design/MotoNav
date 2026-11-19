#!/usr/bin/env python3
"""musicA.109: V2 re-cut of the IDLE (waiting-for-cable) animation.

V2 = the re4_t10p2 candidate the user picked on 2026-11-19:
  * re-cut from the ORIGINAL white-background JPG (not the already-cut sheet)
  * white backdrop = near-white pixels connected to the canvas border
  * leg-gap floor tiles = low-sat mid-dark bluish grey -> clear them even though
    they sit next to the subjects
  * solid subject core excludes that grey family, so it is not protected
  * blue-grey diamond tiles cleared away from the subject
  * tile_thr=10, protect=2

Only ShenhuaCenterIdle.h is regenerated.  ShenhuaCenterConn.h stays as .108
(the connected-state animation has no leg-gap problem).
Pipeline after the re-cut is the .108 one: unified bottom-aligned canvas,
crop y>=24, premultiplied-alpha LANCZOS resize to 189x135, 32-colour palette
on OPAQUE pixels only, RGB565/chroma-0 transparent.
"""
from __future__ import annotations
import sys
from pathlib import Path
import numpy as np
from PIL import Image
from scipy import ndimage

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import bake_new_center_frames as B

JPG = Path(r"C:\Users\coolmanman\.workbuddy\blobs\09\0906528d1352fb194d676e72ac429fb8685d822bd94b10aad186fcdf7e8a4eb1.jpg")
BOUNDS = [0, 232, 457, 677, 892, 1119, 1338, 1554, 1761]  # idle, true boundaries
CW, CH, FRAMES = B.CW, B.CH, B.FRAMES
PREV = HERE.parent / "_bake_preview"
PREV.mkdir(exist_ok=True)

jpg = np.array(Image.open(JPG).convert("RGB")).astype(np.int16)


def white_bg(mn):
    w = mn > 232
    lab, _ = ndimage.label(w, structure=np.ones((3, 3)))
    border = set(lab[0, :]) | set(lab[-1, :]) | set(lab[:, 0]) | set(lab[:, -1])
    bg = np.zeros_like(w)
    for i in border:
        if i:
            bg |= (lab == i)
    return bg


def recut_alpha(f, tile_thr=10, prot=2):
    R, G, Bl = f[..., 0], f[..., 1], f[..., 2]
    mn, mx = f.min(2), f.max(2)
    sat, lum = mx - mn, f.mean(2)
    bg = white_bg(mn)

    # V2: leg-gap floor tiles (low-sat, mid-dark, slightly bluish grey)
    leggap = ((sat <= 42) & (lum >= 96) & (lum <= 134) &
              (Bl >= R + 8) & (Bl >= G + 3) & (R >= 78) & (G >= 78))

    # solid subject core: vivid colours / warm skin / dark outlines,
    # explicitly excluding the grey tile family so it is never protected
    solid = ((sat > 74) | (R > G + 24) | (lum < 96)) & ~leggap
    nearwhite = mn > 234
    dil_solid = ndimage.binary_dilation(solid, structure=np.ones((3, 3)), iterations=3)
    enclosed_white = nearwhite & dil_solid
    solid = solid | enclosed_white
    protect = ndimage.binary_dilation(solid, structure=np.ones((3, 3)), iterations=prot)

    bright = ((Bl >= R + tile_thr) & (Bl >= G + 3) & (lum >= 138) &
              (lum <= 236) & (sat <= 72) & (R >= 100))
    tile = (bright | leggap) & ~protect
    alpha = np.where(bg | tile, 0, 255).astype(np.uint8)
    return np.dstack([np.clip(f, 0, 255).astype(np.uint8), alpha])


def unified(raws):
    canv = []
    for f in raws:
        ys, xs = np.where(f[..., 3] > 16)
        if len(xs):
            f = f[:, xs.min():xs.max() + 1]
        canv.append(f)
    mh = max(f.shape[0] for f in canv)
    mw = max(f.shape[1] for f in canv)
    cc = []
    for f in canv:
        c = np.zeros((mh, mw, 4), np.uint8)
        c[mh - f.shape[0]:, :f.shape[1]] = f
        cc.append(c)
    x0 = min(int(np.where(f[..., 3] > 16)[1].min()) for f in cc)
    x1 = max(int(np.where(f[..., 3] > 16)[1].max()) for f in cc)
    y1 = max(int(np.where(f[..., 3] > 16)[0].max()) for f in cc)
    y0 = 24
    bw, bh = x1 - x0 + 1, y1 - y0 + 1
    s = min(CW / bw, CH / bh)
    nw, nh = round(bw * s), round(bh * s)
    ox, oy = (CW - nw) // 2, (CH - nh) // 2
    out = []
    for c in cc:
        tile = np.zeros((CH, CW, 4), np.uint8)
        tile[oy:oy + nh, ox:ox + nw] = B.resize_rgba_straight(
            Image.fromarray(c[y0:y1 + 1, x0:x1 + 1]), nw, nh)
        out.append(tile)
    print(f"unified: x{x0}-{x1} y{y0}-{y1}, scale {s:.3f} -> {nw}x{nh} at ({ox},{oy})")
    return out


def main():
    raws = []
    for k in range(FRAMES):
        raws.append(recut_alpha(jpg[:, BOUNDS[k]:BOUNDS[k + 1]]))
    frames = unified(raws)

    rgb_list = [im[..., :3].copy() for im in frames]
    valids = [im[..., 3] >= 128 for im in frames]
    pal = B.shared_palette(rgb_list, valids, 32)
    payloads, previews = [], []
    for k in range(FRAMES):
        qrgb = B.apply_palette(rgb_list[k], pal)
        raw = np.frombuffer(B.pack565(qrgb, valids[k]), "<u2").reshape(CH, CW)
        payloads.append(raw.astype("<u2").tobytes())
        rr = (((raw >> 11) & 0x1F).astype(np.uint32) * 255 // 31).astype(np.uint8)
        gg = (((raw >> 5) & 0x3F).astype(np.uint32) * 255 // 63).astype(np.uint8)
        bb = ((raw & 0x1F).astype(np.uint32) * 255 // 31).astype(np.uint8)
        previews.append(np.dstack([rr, gg, bb, np.where(raw > 0, 255, 0).astype(np.uint8)]))
    out = HERE.parent / "firmware" / "boards" / "175" / "MotoNav" / "resources" / "ShenhuaCenterIdle.h"
    B.emit_header(out, "SHENHUA_IDLE", CW, CH, FRAMES, 3500, payloads, "musicA.109 V2 recut")

    mag = np.array([255, 0, 255], np.uint8)
    tiles = []
    for rgba in previews:
        al = rgba[..., 3].astype(float) / 255
        tiles.append((rgba[..., :3] * al[..., None] + mag * (1 - al[..., None])).astype(np.uint8))
    grid = np.concatenate([np.concatenate(tiles[r * 4:(r + 1) * 4], axis=1) for r in range(2)], axis=0)
    prev = PREV / "idle_565_109.png"
    Image.fromarray(grid).resize((grid.shape[1] * 2, grid.shape[0] * 2), Image.NEAREST).save(prev)
    print("preview ->", prev)


if __name__ == "__main__":
    main()
