"""
slice_ranks.py — turn the Rocket League "Competitive Ranks" chart into
the 8 tier PNGs bonezmod expects.

Input:  the sheet with 8 vertical tier columns (bronze -> supersonic
        legend) and 3 rows (rank I, II, III top to bottom).
Output: bronze.png, silver.png, gold.png, platinum.png, diamond.png,
        champion.png, grand_champion.png, supersonic_legend.png
        — each cropped from the top-row (Rank I) badge, background
        transparency removed, saved into --out.

Usage:
    python tools/slice_ranks.py --in ranks_chart.png --out assets/ranks
    python tools/slice_ranks.py --in ranks_chart.png --out %APPDATA%\\bonezmod\\ranks

Deps: pillow  (pip install pillow)

The slicer expects the sheet's 8 tier columns to be equally spaced
across the image width. If your source chart has a large title band
across the top (like the "COMPETITIVE RANKS" strip), pass --top-crop
in pixels or as a fraction (e.g. --top-crop 0.15).
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.stderr.write("pillow required: pip install pillow\n")
    sys.exit(1)

TIER_ORDER = [
    "bronze",
    "silver",
    "gold",
    "platinum",
    "diamond",
    "champion",
    "grand_champion",
    "supersonic_legend",
]


def parse_frac(v: str) -> float:
    v = v.strip()
    if v.endswith("%"):
        return float(v[:-1]) / 100.0
    f = float(v)
    return f if f < 1.0 else f  # caller decides if it's pixels or fraction


def crop_column(img: Image.Image, i: int, cols: int,
                top_frac: float, row_frac: float) -> Image.Image:
    w, h = img.size
    col_w = w / cols
    x0 = int(i * col_w)
    x1 = int((i + 1) * col_w)
    y_top = int(h * top_frac)
    y_bot = int(y_top + (h - y_top) * row_frac)
    return img.crop((x0, y_top, x1, y_bot))


def strip_background(img: Image.Image, tolerance: int = 40) -> Image.Image:
    """
    Very cheap background remove: assume the four corners are background
    color, flood-mask by color similarity, set matched pixels to alpha 0.
    """
    img = img.convert("RGBA")
    px = img.load()
    w, h = img.size
    corners = [px[0, 0], px[w - 1, 0], px[0, h - 1], px[w - 1, h - 1]]
    # median-ish reference: pick the corner with the lowest luminance —
    # RL sheet backgrounds are dark tier gradients.
    ref = min(corners, key=lambda c: c[0] + c[1] + c[2])
    rr, rg, rb, _ = ref
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if abs(r - rr) + abs(g - rg) + abs(b - rb) <= tolerance * 3:
                px[x, y] = (r, g, b, 0)
    return img


def autotrim(img: Image.Image, margin: int = 4) -> Image.Image:
    bbox = img.getbbox()
    if not bbox:
        return img
    l, t, r, b = bbox
    l = max(0, l - margin)
    t = max(0, t - margin)
    r = min(img.size[0], r + margin)
    b = min(img.size[1], b + margin)
    return img.crop((l, t, r, b))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="src", required=True, help="input sheet PNG")
    ap.add_argument("--out", dest="dst", required=True, help="output directory")
    ap.add_argument("--top-crop", default="0.14",
                    help="fraction of image height to drop from top (title band)")
    ap.add_argument("--row-frac", default="0.36",
                    help="height fraction of one badge row after top crop")
    ap.add_argument("--tolerance", type=int, default=40,
                    help="background-color match tolerance per channel")
    ap.add_argument("--keep-bg", action="store_true",
                    help="skip transparent background removal")
    args = ap.parse_args()

    src = Path(args.src)
    dst = Path(os.path.expandvars(args.dst))
    dst.mkdir(parents=True, exist_ok=True)

    top_frac = parse_frac(args.top_crop)
    row_frac = parse_frac(args.row_frac)

    img = Image.open(src).convert("RGBA")
    for i, name in enumerate(TIER_ORDER):
        cell = crop_column(img, i, len(TIER_ORDER), top_frac, row_frac)
        if not args.keep_bg:
            cell = strip_background(cell, tolerance=args.tolerance)
        cell = autotrim(cell)
        out = dst / f"{name}.png"
        cell.save(out, "PNG")
        print(f"[+] {out}  ({cell.size[0]}x{cell.size[1]})")

    print(f"done. {len(TIER_ORDER)} badges written to {dst}")


if __name__ == "__main__":
    main()
