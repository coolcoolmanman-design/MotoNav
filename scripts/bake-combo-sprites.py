#!/usr/bin/env python3
"""Bake Combo_* Assets.xcassets from confirmed mockup PNGs (center-slot-combo-sprite-mockup).

Run from repo root with Pillow. Source mockups expected at:
  /cursor/stores/self/media/center-slot-combo-sprite-mockup/
Override with COMBO_MOCKUP_DIR.
"""
from __future__ import annotations
import json, os
from pathlib import Path
import numpy as np
from PIL import Image

MEDIA = Path(os.environ.get(
    "COMBO_MOCKUP_DIR",
    "/cursor/stores/self/media/center-slot-combo-sprite-mockup",
))
ASSETS = Path(__file__).resolve().parents[1] / "ios/MotoNav/Assets.xcassets"
VEHICLES = ["sedan", "suv", "supercar", "motorcycle", "scooter", "sportbike", "bicycle", "foldingBike"]


def write_imageset(name: str, im: Image.Image) -> None:
    d = ASSETS / f"{name}.imageset"
    d.mkdir(parents=True, exist_ok=True)
    im.save(d / f"{name}.png", optimize=True)
    (d / "Contents.json").write_text(json.dumps({
        "images": [
            {"filename": f"{name}.png", "idiom": "universal", "scale": "1x"},
            {"idiom": "universal", "scale": "2x"},
            {"idiom": "universal", "scale": "3x"},
        ],
        "info": {"author": "xcode", "version": 1},
    }, indent=2))


def content_bbox(im: Image.Image, thr: int = 18):
    a = np.array(im.convert("RGB"))
    mask = a.sum(2) > thr
    ys, xs = np.where(mask)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def to_466(sprite: Image.Image, max_w: int = 210, safe_h: int = 198) -> Image.Image:
    im = sprite.convert("RGB")
    bb = content_bbox(im)
    if not bb:
        return Image.new("RGB", (466, 466), (0, 0, 0))
    x0, y0, x1, y1 = bb
    spr = im.crop((x0, y0, x1 + 1, y1 + 1))
    sw, sh = spr.size
    scale = min(max_w / sw, safe_h / sh, 1.0)
    nw = max(1, int(round(sw * scale)))
    nh = max(1, int(round(sh * scale)))
    spr = spr.resize((nw, nh), Image.NEAREST)
    canvas = Image.new("RGB", (466, 466), (0, 0, 0))
    canvas.paste(spr, ((466 - nw) // 2, 150 + max(0, (safe_h - nh) // 2)))
    return canvas


def scrub_hud(im: Image.Image) -> Image.Image:
    a = np.array(im.convert("RGB"))
    h, w, _ = a.shape
    lum = a.astype(np.int16).sum(2)
    for y in list(range(0, min(145, h))) + list(range(max(0, h - 130), h)):
        for x in range(w):
            if lum[y, x] > 480:
                a[y, x] = 0
    for x in list(range(0, 55)) + list(range(w - 55, w)):
        for y in range(h):
            if lum[y, x] > 480:
                a[y, x] = 0
    return Image.fromarray(a)


def scrub_labels(im: Image.Image) -> Image.Image:
    a = np.array(im.convert("RGB"))
    h, w, _ = a.shape
    lum = a.astype(np.int16).sum(2)
    r = a[:, :, 0].astype(int)
    g = a[:, :, 1].astype(int)
    b = a[:, :, 2].astype(int)
    sat = np.maximum(np.maximum(r, g), b) - np.minimum(np.minimum(r, g), b)
    yy, xx = np.mgrid[:h, :w]
    rad2 = (xx - w // 2) ** 2 + (yy - h // 2) ** 2
    white = lum >= 620
    gray_arc = (lum > 280) & (lum < 520) & (sat < 20) & (rad2 > 150 ** 2)
    lower_gray = (yy > 330) & (lum > 120) & (sat < 30)
    a[white | gray_arc | lower_gray] = 0
    return Image.fromarray(a)


def extract_three(path: Path):
    src = Image.open(path).convert("RGB")
    boxes = [(15, 90, 247, 265), (252, 90, 485, 265), (493, 90, 701, 265)]
    return [src.crop(b) for b in boxes]


def from_round(name: str) -> Image.Image:
    src = scrub_hud(Image.open(MEDIA / name))
    return to_466(src.crop((40, 115, 426, 370)), max_w=200)


def col_centers(gray, y0, y1):
    band = gray[y0:y1]
    black = (band < 30).mean(0)
    mid = ((band >= 30) & (band < 200)).mean(0)
    score = black * 0.5 + mid
    sm = np.convolve(score, np.ones(21) / 21, mode="same")
    peaks = []
    for x in range(40, 680):
        if sm[x] > 0.25 and sm[x] >= sm[x - 1] and sm[x] >= sm[x + 1]:
            if not peaks or x - peaks[-1] > 80:
                peaks.append(x)
            elif sm[x] > sm[peaks[-1]]:
                peaks[-1] = x
    return peaks[:4]


def extract_sprite(arr, cx, cy, r=76) -> Image.Image:
    crop = arr[cy - r: cy + r, cx - r: cx + r].copy()
    h, w, _ = crop.shape
    yy, xx = np.mgrid[:h, :w]
    circ = (xx - r) ** 2 + (yy - r) ** 2 <= (r - 6) ** 2
    lum = crop.astype(np.int16).sum(2)
    keep = circ & (lum > 40) & (lum < 700)
    kept = keep.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            kept |= np.roll(np.roll(keep, dy, 0), dx, 1)
    kept &= circ
    out = np.zeros_like(crop)
    out[kept] = crop[kept]
    ys, xs = np.where(kept)
    if len(xs) == 0:
        return Image.new("RGB", (1, 1), (0, 0, 0))
    return Image.fromarray(out[ys.min(): ys.max() + 1, xs.min(): xs.max() + 1])


def main() -> None:
    assert MEDIA.is_dir(), MEDIA
    ASSETS.mkdir(parents=True, exist_ok=True)

    for src, g, st in [
        ("05a-cars-male-wave-incabin.png", "m", "wave"),
        ("05b-cars-female-wave-incabin.png", "f", "wave"),
        ("05c-cars-male-sleep-incabin.png", "m", "sleep"),
        ("05d-cars-female-sleep-incabin.png", "f", "sleep"),
    ]:
        for veh, crop in zip(["sedan", "suv", "supercar"], extract_three(MEDIA / src)):
            write_imageset(f"Combo_{g}_{veh}_{st}", scrub_labels(to_466(crop, max_w=210)))

    write_imageset("Combo_m_motorcycle_sleep", scrub_labels(from_round("07-round-466-male-moto-offline.png")))
    write_imageset("Combo_m_motorcycle_wave", scrub_labels(from_round("08-round-466-male-moto-online.png")))
    write_imageset("Combo_f_scooter_sleep", scrub_labels(from_round("09-round-466-female-scooter-offline.png")))
    write_imageset("Combo_f_scooter_wave", scrub_labels(from_round("10-round-466-female-scooter-online.png")))

    matrix = Image.open(MEDIA / "05-vehicle-matrix-thumbs.png").convert("RGB")
    arr = np.array(matrix)
    gray = arr.mean(2)
    rows = [
        ("m", 250, 380, 315, ["sedan", "suv", "supercar", "motorcycle"]),
        ("m", 560, 700, 630, ["scooter", "sportbike", "bicycle", "foldingBike"]),
        ("f", 990, 1110, 1050, ["sedan", "suv", "supercar", "motorcycle"]),
        ("f", 1310, 1430, 1370, ["scooter", "sportbike", "bicycle", "foldingBike"]),
    ]
    two = {"motorcycle", "scooter", "sportbike", "bicycle", "foldingBike"}
    for g, y0, y1, cy, vehs in rows:
        xs = col_centers(gray, y0, y1)
        for cx, veh in zip(xs, vehs):
            if veh not in two:
                continue
            if (g == "m" and veh == "motorcycle") or (g == "f" and veh == "scooter"):
                continue
            canvas = scrub_labels(to_466(extract_sprite(arr, cx, cy), max_w=200))
            write_imageset(f"Combo_{g}_{veh}_wave", canvas)
            write_imageset(f"Combo_{g}_{veh}_sleep", canvas)

    missing = []
    for g in "mf":
        for v in VEHICLES:
            for st in ("sleep", "wave"):
                p = ASSETS / f"Combo_{g}_{v}_{st}.imageset" / f"Combo_{g}_{v}_{st}.png"
                if not p.is_file():
                    missing.append(str(p))
    if missing:
        raise SystemExit(f"missing assets: {missing}")
    print(f"baked 32 Combo_* imagesets into {ASSETS}")


if __name__ == "__main__":
    main()
