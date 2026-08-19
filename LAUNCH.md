# bonezmod — launch guide

External, EAC-safe Rocket League companion. No injection, no reads/writes
against `RocketLeague.exe`. Everything runs in its own process.

---

## Feature list

### HUD / overlay (`overlay/`)
- Transparent, click-through, always-on-top overlay pinned to the RL window.
- Status bar: RL pid, windowed/fullscreen, pad state (connected/armed/disarmed), EAC state.
- Red EAC-armed bar across the bottom when the anti-cheat guard trips.
- Optional ball ring + fading trail (vision-based).

### EAC guard (`eac_guard`)
- Probes `EasyAntiCheat` / `EasyAntiCheat_EOS` services, launcher processes, and RL's loaded modules.
- Access-denied on RL's module snapshot counts as positive (protected-process hardening).
- When armed, all input-emitting plugins are gated off automatically.

### Vision (`capture/dxgi_capture`, `vision/detector`)
- DXGI Desktop Duplication on the monitor RL is on. Zero contact with the game.
- OpenCV HoughCircles for ball detection; HSV mask for boost meter.
- Feeds ball trail, mirror, auto-queue, HUD.

### Virtual pad (`input/vigem_pad`)
- ViGEmBus virtual Xbox 360 controller. RL sees a real gamepad; EAC does not distinguish.
- All macros / auto-actions ride this channel.

### Macro engine (`input/macro`)
- Preemptable worker thread running timed `PadState` scripts.
- Prebuilt library: freeplay shot reset, full exit-to-freeplay, ball-cam toggle, pause, quick chat.

### Plugins (`plugins/`)
| Plugin | What it does |
|---|---|
| `freeplay_reset` | shot reset + full menu-nav back to freeplay |
| `training_pack` | type pack code (keyboard or virtual OSK), next/prev shot |
| `camera_preset` | menu-nav slider driver with `kSquishy` / `kJstn` / `kRizzo` presets |
| `workshop_manager` | file-swap `.udk` maps into RL's `\CookedPCConsole\mods\` |
| `ball_trail` | 40-sample fading polyline of recent ball positions |
| `obs_replay` | obs-websocket v5 client, saves the OBS replay buffer on hotkey |
| `mmr_reveal` | live lobby MMR card: log-tail roster + tracker.gg profile fetch + rank badge + smurf marker |

### Offline
- `replay/replay_parser` — parses `.replay` header + property bag (map, playlist, score, players).
- `api/tracker_api` — WinHTTP profile fetch from tracker.gg.
- `api/log_watcher` — tails `Launch.log` for lobby membership.

### Rank badges (`overlay/rank_badge`)
- WIC-loaded PNG per tier if you have them; procedural D2D silhouette (bronze circle, silver hex, gold chevron crest, platinum star, diamond, champion horns, GC wings, SSL spear) otherwise.
- Search order: `%APPDATA%\bonezmod\ranks\` → `<exe_dir>\assets\ranks\` → procedural.
- `tools/slice_ranks.py` cuts the RL competitive-ranks chart into the 8 named PNGs.

---

## Hotkeys

All chorded with `Ctrl+Alt` to avoid clashing with in-game binds.

| Combo | Action |
|---|---|
| `R` | full exit-to-freeplay macro |
| `E` | shot reset (training / freeplay) |
| `C` | apply Squishy camera preset |
| `S` | save OBS replay buffer (obs-websocket) |
| `T` | show / hide overlay |
| `M` | toggle click-through |
| `F1` | toggle ball trail |
| `F3` | toggle MMR reveal card |
| `Q` | quit bonezmod |

---

## Build

### Prereqs
- **Windows 10 / 11 x64**
- **Visual Studio 2022** (Desktop C++ workload — MSVC v143, Win10/11 SDK)
- **CMake ≥ 3.24**
- **ViGEmBus driver** — https://github.com/nefarius/ViGEmBus/releases (installer, one-time)
- **OpenCV 4.x** (optional but recommended — provides ball / boost detection). Easiest install: `winget install opencv` or grab the prebuilt from opencv.org and set `OpenCV_DIR` to the `build\` folder.
- **Python + Pillow** (optional, only if you're running the rank-chart slicer): `pip install pillow`

### Vendored dependencies
Clone or drop into `third_party/`:

```
third_party/
  ViGEmClient/           # https://github.com/nefarius/ViGEmClient
  imgui/                 # optional; only if you want the ImGui panel
```

If either is absent, CMake auto-disables that feature (virtual pad falls back to stubs, ImGui panel is skipped) and still builds. **ViGEmClient is required if you want input to actually work.**

### Configure + build

```powershell
git clone https://github.com/ombdeveloping/bonezmod
cd bonezmod
git checkout claude/rl-bakkesmod-eac-safe-hyyc1x

git clone https://github.com/nefarius/ViGEmClient third_party/ViGEmClient

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
      -DOpenCV_DIR="C:\opencv\build"

cmake --build build --config Release
```

Binary lands at `build\Release\bonezmod.exe`.

### Optional: rank badge PNGs

```powershell
pip install pillow
python tools\slice_ranks.py --in path\to\ranks_chart.png --out assets\ranks
```

Or drop your own eight files into `assets\ranks\` with the names listed in `assets\ranks\README.md`.

---

## Run

1. Install ViGEmBus driver (one-time).
2. Launch **Rocket League** to the main menu.
3. Launch `bonezmod.exe`. The transparent overlay appears above RL.
4. Confirm the status line shows `pad: armed` and `eac: clear`. If it shows `eac: [service]` / `[launcher]` / `[module: …]`, the pad self-disarms; hotkeys will not send input. That's by design.
5. Try `Ctrl+Alt+E` in freeplay — the car should reset shot. If it doesn't, jump to Troubleshooting.

---

## Troubleshooting

**Overlay doesn't appear.** Something else is on top with `WS_EX_NOREDIRECTIONBITMAP` (e.g. NVIDIA overlay). Toggle it off or bump bonezmod with `Ctrl+Alt+T`.

**Pad shows `disconnected`.** ViGEmBus driver isn't installed or the service didn't start. Reboot after installing the driver.

**Pad shows `safe-disarmed`.** EAC guard tripped. Check the `eac: […]` field in the status line for which signal fired. This is protection — do not disable it.

**Macros fire but game doesn't respond.** Your controller-slot priority may be wrong. In RL controls, ensure the virtual "Xbox 360 Controller" is bound. In multi-pad setups, unplug others while testing.

**OpenCV features silent.** You built without `-DOpenCV_DIR`; ball ring / trail / auto-queue vision are stubs. Rebuild with OpenCV.

**OBS clip hotkey does nothing.** obs-websocket v5 must be enabled in OBS (Tools → WebSocket Server Settings), password set to blank or matching `obs.configure(host, port, pw)` in `main.cpp`, and the replay buffer must be started.

**Camera preset takes forever.** By default `HK_CAM_APPLY` skips the slider reset. If you never navigate to Camera settings first, the sliders won't be at the expected start positions — walk to Options → Camera once, then hit the hotkey.

**MMR reveal card is empty in-lobby.** Log tail depends on RL patch line format; if a new patch changed strings, regex in `src/api/log_watcher.cpp` needs a tweak. Names still populate from tracker.gg once they appear in the log.

**RL patch breaks a macro.** Menu timings in `src/input/macro.cpp` and `src/plugins/camera_preset.cpp` are the retune targets.

---

## Safety reminder

Every plugin that emits input is gated on:

```
pad_ok = pad_connected && vpad_arm && !eac_warning
```

If EAC ever registers as active, no macros fire and no pad state is submitted. The overlay keeps painting (it never touches RL) and vision keeps analyzing (it never touches RL). That's the guarantee.
