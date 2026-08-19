# bonezmod — launch guide

Read-only MMR reveal overlay for Rocket League. Reads your client log
and tracker.gg; paints a per-player rank card on a transparent overlay.

No injection, no memory access, no input emission. `RocketLeague.exe`
is never opened.

---

## Features

- Transparent, click-through overlay pinned to the RL window
- Per-player card: team color · rank badge · handle · MMR · peak · W/G
- Rank badge: WIC-loaded PNG if you supply one, procedural D2D
  silhouette otherwise (bronze circle, silver hex, gold chevron crest,
  platinum star, diamond, champion horns, GC wings, SSL spear)
- Smurf flag: red `!` on the badge when `games < 40 && mmr > 1000`
- Log-tail lobby watcher (Documents\My Games\Rocket League\...\Launch.log)
- tracker.gg profile fetch on a worker thread with per-session cache
- EAC guard runs but is informational only (nothing to disarm)

## Hotkeys

| Combo | Action |
|---|---|
| `Ctrl+Alt+T`  | show / hide overlay |
| `Ctrl+Alt+F3` | toggle MMR card |
| `Ctrl+Alt+Q`  | quit |

## Build

Prereqs: Windows 10/11 x64, Visual Studio 2022 (Desktop C++), CMake ≥ 3.24.

```powershell
git clone https://github.com/ombdeveloping/bonezmod
cd bonezmod
git checkout claude/rl-bakkesmod-eac-safe-hyyc1x

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Binary: `build\Release\bonezmod.exe`. No external dependencies at
runtime — just Windows.

## Optional rank badge PNGs

```powershell
pip install pillow
python tools\slice_ranks.py --in path\to\ranks_chart.png --out assets\ranks
```

Or drop your own into `%APPDATA%\bonezmod\ranks\` with names:
`bronze.png silver.png gold.png platinum.png diamond.png champion.png
grand_champion.png supersonic_legend.png`. Missing files fall back to
the procedural D2D shape.

## Run

1. Launch Rocket League.
2. Launch `bonezmod.exe`.
3. Queue into a match. Names appear as the log emits joins; MMR badges
   fill in a second later as tracker.gg responds.

## Safety

Nothing this binary does can be seen by EAC or by Psyonix's behavior
heuristics:

- overlay is its own HWND, drawn via DirectComposition, click-through
- log tail opens `Launch.log` with shared read/write/delete
- tracker.gg is regular HTTPS from your machine
- no `OpenProcess`, no `WriteProcessMemory`, no injection primitives
- no input emission — no virtual pad, no `SendInput`

Same risk class as having tracker.gg open on a second monitor.
