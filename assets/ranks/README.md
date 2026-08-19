# Rank badge PNGs

`bonezmod` looks for these eight files, in this order:

1. `%APPDATA%\bonezmod\ranks\<family>.png`   (per-user override)
2. `<exe_dir>\assets\ranks\<family>.png`      (shipped default)

Filenames (case-sensitive on some setups; keep lower-case):

```
bronze.png
silver.png
gold.png
platinum.png
diamond.png
champion.png
grand_champion.png
supersonic_legend.png
```

Anything missing falls back to the procedural D2D shield rendered by
`overlay/rank_badge.cpp`.

## Generating from the official ranks chart

Use the slicer:

```
pip install pillow
python tools/slice_ranks.py --in path\to\ranks_chart.png --out assets/ranks
```

The slicer crops the top row (Rank I badge) of each of the 8 tier
columns, strips the dark background via corner-color match, and
auto-trims to the badge bbox. If your chart has a different top band
height, pass e.g. `--top-crop 0.10 --row-frac 0.40`.
