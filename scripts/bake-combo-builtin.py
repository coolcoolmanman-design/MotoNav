#!/usr/bin/env python3
"""Bake the 16 Combo presets into firmware built-in PROGMEM frames.

The original mockups live outside this repo, so this script re-processes the
32 committed 466x466 Combo_*.png assets instead:

  * build an EXPLICIT alpha mask from the largest connected foreground blob,
    then fill its outer contour so dark parts INSIDE the subject (tinted
    windows, black tyres, frames, outlines, dark clothing) stay opaque.
    musicA.57: the old code used luminance >= 20 as "opaque", which punched
    holes through every dark design element ("图案元素透明缺失").
  * union the sleep/wave bounding boxes so both roles share one framing
  * re-fit per vehicle: cars 236x150, two-wheelers 210x186
  * emit 256x192 RGB565 little-endian frames, chroma key 0x0000;
    opaque pure black is encoded as 0x0001 sentinel
  * zlib level 9 -> firmware/boards/175/MotoNav/resources/ComboBuiltin.h
  * write back cleaned, upright 466x466 PNGs for the iOS preview
  * COMBO_SAME_MASK: bit per preset whose sleep/wave source art is identical

Idempotent: pristine sources are copied to scripts/combo-src/ on first run and
are always used as input. Re-running produces byte-identical output.

Run from anywhere:  python scripts/bake-combo-builtin.py
"""
from __future__ import annotations
import hashlib
import json
import shutil
import zlib
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "ios" / "MotoNav" / "Assets.xcassets"
SRC = ROOT / "scripts" / "combo-src"
OUT_H = ROOT / "firmware" / "boards" / "175" / "MotoNav" / "resources" / "ComboBuiltin.h"
MANIFEST = ROOT / "scripts" / "combo-builtin-manifest.json"

GENDERS = ["m", "f"]
VEHICLES = ["sedan", "suv", "supercar", "motorcycle", "scooter",
            "sportbike", "bicycle", "foldingBike"]
ROLES = ["sleep", "wave"]
CARS = {"sedan", "suv", "supercar"}
W, H = 256, 192
FRAME_BYTES = W * H * 2
ALPHA_CUT = 80          # scaled mask: >= 80 is opaque (chroma key has no AA)


def combo_name(g: str, v: str, r: str) -> str:
    return f"Combo_{g}_{v}_{r}.png"


def bootstrap_src() -> None:
    if SRC.is_dir() and len(list(SRC.glob("Combo_*.png"))) == 32:
        return
    SRC.mkdir(parents=True, exist_ok=True)
    for f in sorted(ASSETS.glob("Combo_*.imageset/*.png")):
        shutil.copy2(f, SRC / f.name)
    n = len(list(SRC.glob("Combo_*.png")))
    if n != 32:
        raise RuntimeError(f"expected 32 source PNGs, got {n}")


def lum(a: np.ndarray) -> np.ndarray:
    return a[..., 0] * .299 + a[..., 1] * .587 + a[..., 2] * .114


def clean_full(path: Path):
    """Return (cleaned_rgb, alpha_mask, bbox, area).

    The source is a black round-screen capture. Dark design elements inside
    the subject must stay, so foreground is seeded generously and the largest
    blob's OUTER contour is filled to close interior holes (windows/tyres).
    """
    a = np.array(Image.open(path).convert("RGB"))
    L = lum(a)
    chroma = a.max(2).astype(int) - a.min(2).astype(int)
    # Loose seed: dark grey glass / near-black outlines still count, while
    # the flat (12,14,18) panel and pure-black background do not.
    fg = ((L >= 18) | (chroma >= 14)).astype(np.uint8) * 255
    k7 = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))
    k5 = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    k3 = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    fg = cv2.morphologyEx(fg, cv2.MORPH_CLOSE, k7)
    fg = cv2.dilate(fg, k5, iterations=1)
    n, lab, stats, cent = cv2.connectedComponentsWithStats(fg, 8)
    best, best_score = 0, -1e9
    for i in range(1, n):
        area = int(stats[i, cv2.CC_STAT_AREA])
        cx, cy = cent[i]
        d = float(np.hypot(cx - 233, cy - 240))
        score = float(area) - d * 30.0
        if score > best_score:
            best_score, best = score, i
    if best == 0:
        raise RuntimeError(f"no foreground blob in {path}")
    blob = (lab == best).astype(np.uint8) * 255
    # Fill every interior hole, then pull the dilate back in a little.
    contours, _ = cv2.findContours(blob, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    filled = np.zeros_like(blob)
    cv2.drawContours(filled, contours, -1, 255, thickness=cv2.FILLED)
    filled = cv2.erode(filled, k3, iterations=1)
    mask = filled > 0
    cleaned = a.copy()
    cleaned[~mask] = 0
    # NOTE: do NOT zero dark pixels inside the mask any more.
    ys, xs = np.where(mask)
    bbox = (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))
    return cleaned, mask, bbox, int(stats[best, cv2.CC_STAT_AREA])


def to565(a: np.ndarray, valid: np.ndarray) -> np.ndarray:
    r = a[..., 0].astype(np.uint16) >> 3
    g = a[..., 1].astype(np.uint16) >> 2
    b = a[..., 2].astype(np.uint16) >> 3
    c = (r << 11) | (g << 5) | b
    c[~valid] = 0                       # transparent key
    c[valid & (c == 0)] = 1            # opaque-black sentinel 0x0001
    return c.astype("<u2")


def fit_pair(g: str, v: str):
    """Return (sleep_bytes, wave_bytes, sleep_preview, wave_preview)."""
    cs, ms, bs, as_ = clean_full(SRC / combo_name(g, v, "sleep"))
    cw, mw, bw, aw = clean_full(SRC / combo_name(g, v, "wave"))
    x0 = max(0, min(bs[0], bw[0]) - 2)
    y0 = max(0, min(bs[1], bw[1]) - 2)
    x1 = min(465, max(bs[2], bw[2]) + 2)
    y1 = min(465, max(bs[3], bw[3]) + 2)
    crop_s = cs[y0:y1 + 1, x0:x1 + 1].copy()
    crop_w = cw[y0:y1 + 1, x0:x1 + 1].copy()
    mask_s = ms[y0:y1 + 1, x0:x1 + 1].copy()
    mask_w = mw[y0:y1 + 1, x0:x1 + 1].copy()
    # f_scooter_sleep comes from a dark round-screen capture whose blob is
    # tiny; fall back to the wave source so both roles show the same rider.
    if as_ < aw * 0.35:
        crop_s = crop_w.copy()
        mask_s = mask_w.copy()

    ch, cw_px = crop_s.shape[:2]
    max_w, max_h = (236, 150) if v in CARS else (210, 186)
    s = min(max_w / cw_px, max_h / ch)
    nw, nh = max(1, round(cw_px * s)), max(1, round(ch * s))
    ox, oy = (W - nw) // 2, (H - nh) // 2

    def make(crop, cmask):
        r = np.array(Image.fromarray(crop).resize((nw, nh), Image.LANCZOS))
        m = np.array(Image.fromarray(cmask.astype(np.uint8) * 255)
                     .resize((nw, nh), Image.LANCZOS))
        canvas = np.zeros((H, W, 3), np.uint8)
        canvas[oy:oy + nh, ox:ox + nw] = r
        valid = np.zeros((H, W), bool)
        valid[oy:oy + nh, ox:ox + nw] = m >= ALPHA_CUT
        frame = to565(canvas, valid).tobytes()
        # Upright 466x466 PNG for the iOS preview, using the SAME mask so
        # the preview and the round screen show identical opaque areas.
        big = np.array(Image.fromarray(canvas).resize((466, 466), Image.LANCZOS))
        bigm = np.array(Image.fromarray(valid.astype(np.uint8) * 255)
                        .resize((466, 466), Image.LANCZOS))
        big[bigm < ALPHA_CUT] = 0
        return frame, Image.fromarray(big)

    fs, ps = make(crop_s, mask_s)
    fw, pw = make(crop_w, mask_w)
    assert len(fs) == len(fw) == FRAME_BYTES
    return fs, fw, ps, pw


def hex_array(data: bytes) -> list[str]:
    out = []
    for i in range(0, len(data), 16):
        out.append("  " + ",".join("0x%02X" % b for b in data[i:i + 16]) + ",")
    return out


def main() -> None:
    bootstrap_src()
    same_mask = 0
    for gi, g in enumerate(GENDERS):
        for vi, v in enumerate(VEHICLES):
            a = np.array(Image.open(SRC / combo_name(g, v, "sleep")).convert("RGB"))
            b = np.array(Image.open(SRC / combo_name(g, v, "wave")).convert("RGB"))
            if np.array_equal(a, b):
                same_mask |= 1 << (gi * 8 + vi)

    frames = {}
    for gi, g in enumerate(GENDERS):
        for vi, v in enumerate(VEHICLES):
            fs, fw, ps, pw = fit_pair(g, v)
            for ri, (r, payload, prev) in enumerate(zip(ROLES, (fs, fw), (ps, pw))):
                idx = (gi * 8 + vi) * 2 + ri
                z = zlib.compress(payload, 9)
                assert len(z) >= 8 and len(zlib.decompress(z)) == FRAME_BYTES
                frames[idx] = {
                    "g": g, "v": v, "r": r, "z": z,
                    "frame_sha256": hashlib.sha256(payload).hexdigest(),
                }
                dst = ASSETS / f"Combo_{g}_{v}_{r}.imageset" / combo_name(g, v, r)
                dst.parent.mkdir(parents=True, exist_ok=True)
                prev.save(dst, optimize=True)

    ordered = [frames[i] for i in range(32)]
    zlens = [len(f["z"]) for f in ordered]
    zoffs, acc = [], 0
    for n in zlens:
        zoffs.append(acc)
        acc += n
    data = b"".join(f["z"] for f in ordered)

    lines = [
        "// Auto-generated by scripts/bake-combo-builtin.py. Do not edit.",
        "// 16 presets (2 genders x 8 vehicles) x 2 roles = 32 frames.",
        "// 256x192 RGB565 LE, chroma key 0x0000, opaque black = 0x0001.",
        "// Explicit alpha mask (no luminance keying): dark design elements",
        "// such as tinted windows / tyres / outlines stay fully opaque.",
        "// idx = (gender*8 + vehicle)*2 + role;  role 0=sleep 1=wave/chase",
        "#pragma once",
        "#include <stdint.h>",
        "#include <pgmspace.h>",
        "#define COMBO_GENDERS 2",
        "#define COMBO_VEHICLES 8",
        "#define COMBO_ROLES 2",
        "#define COMBO_W 256",
        "#define COMBO_H 192",
        "#define COMBO_FRAME_BYTES 98304u",
        "#define COMBO_FRAME_COUNT 32",
        "#define COMBO_SAME_MASK 0x%04Xu" % same_mask,
        "// COMBO_SAME_MASK: bit (gender*8+vehicle) set when sleep/wave art is identical",
        "#define COMBO_ZTOTAL %d" % len(data),
        "static const uint32_t COMBO_ZLEN[32] = {" + ",".join(str(x) for x in zlens) + "};",
        "static const uint32_t COMBO_ZOFF[32] = {" + ",".join(str(x) for x in zoffs) + "};",
        "static const uint8_t COMBO_ZDATA[] PROGMEM = {",
    ]
    lines.extend(hex_array(data))
    lines.append("};")
    OUT_H.write_bytes(("\r\n".join(lines) + "\r\n").encode("ascii"))

    MANIFEST.write_text(json.dumps({
        "firmwareHeader": str(OUT_H.relative_to(ROOT)).replace("\\", "/"),
        "frameBytes": FRAME_BYTES,
        "width": W, "height": H,
        "alphaCut": ALPHA_CUT,
        "sameMask": same_mask,
        "zTotal": len(data),
        "zSha256": hashlib.sha256(data).hexdigest(),
        "appRemainingBefore": 801904,
        "fits": len(data) < 700 * 1024,
        "frames": [{
            "idx": i, "gender": f["g"], "vehicle": f["v"], "role": f["r"],
            "zlen": len(f["z"]), "zoff": zoffs[i],
            "frameSha256": f["frame_sha256"],
        } for i, f in enumerate(ordered)],
    }, indent=2) + "\n", encoding="utf-8")

    print("frames:", len(ordered))
    print("sameMask: 0x%04X" % same_mask)
    print("zlib total:", len(data), "B =", round(len(data) / 1024, 1), "KiB")
    print("max frame:", max(zlens), "B; min:", min(zlens), "B")
    print("app remaining: 801904 B =", round(801904 / 1024, 1), "KiB")
    print("fits:", len(data) < 700 * 1024)
    print("header:", OUT_H, OUT_H.stat().st_size, "B")
    print("manifest:", MANIFEST)


if __name__ == "__main__":
    main()
