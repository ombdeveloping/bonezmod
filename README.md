# bonezmod

External, EAC-safe Rocket League companion suite. No injection, no
in-process hooks, no memory reads/writes against `RocketLeague.exe`.

## Design

Every subsystem is out-of-process:

| Feature            | Mechanism                                              |
|--------------------|--------------------------------------------------------|
| Game state         | DXGI Desktop Duplication + OpenCV detectors           |
| Input / macros     | ViGEm virtual Xbox 360 pad                             |
| Overlay / HUD      | Standalone transparent DirectComposition + Direct2D   |
| Training resets    | Input macros routed through the virtual pad            |
| Replay analysis    | Offline parse of `.replay` files (psyonix format)      |
| Rank / stats       | Public tracker.gg style REST endpoint                  |
| EAC guard          | Service + loaded-module probe, refuse to arm if EAC on |

## Build

Windows 10+ / VS 2022 / CMake 3.24+.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Deps (fetched by CMake or vendored under `third_party/`):
- ViGEmClient (https://github.com/nefarius/ViGEmClient)
- OpenCV 4.x (world.lib)
- Dear ImGui (docking) with `imgui_impl_win32` + `imgui_impl_dx11`

Runtime deps: ViGEmBus driver installed
(https://github.com/nefarius/ViGEmBus/releases).

## Layout

```
src/
  main.cpp
  eac_guard.{h,cpp}
  process_watch.{h,cpp}
  capture/dxgi_capture.{h,cpp}
  vision/detector.{h,cpp}
  overlay/overlay.{h,cpp}
  input/vigem_pad.{h,cpp}
  input/macro.{h,cpp}
  replay/replay_parser.{h,cpp}
  api/tracker_api.{h,cpp}
  plugins/freeplay_reset.{h,cpp}
  plugins/training_pack.{h,cpp}
  plugins/shot_mirror.{h,cpp}
  ui/controlpanel.{h,cpp}
```
