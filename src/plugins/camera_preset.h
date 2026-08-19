#pragma once
#include "input/macro.h"

namespace bonez::plugins {

struct CameraPreset {
    int fov = 110;          // 60..110
    int height = 100;       // 40..200
    int angle = -3;         // -7..0
    int distance = 270;     // 120..270
    int stiffness = 50;     // 0..100 (x100 in-game)
    int swivel = 250;       // 100..1000 (x100 in-game)
    int transition = 100;   // 0..200 (x100 in-game)
    bool ball_cam_toggle = true;
    bool invert_swivel = false;
};

extern const CameraPreset kSquishy;   // very common competitive preset
extern const CameraPreset kJstn;
extern const CameraPreset kRizzo;

// Navigates: pause → Settings → Camera, sets each slider by comparing
// requested value to a known-default step model, then backs out. Timings
// assume default menu animation speed.
//
// NOTE: This is a nav macro; it cannot READ the current slider values.
// Use `reset_to_zero_first=true` (default) to fully left-pull each
// slider before stepping right so the resulting position is deterministic.
void apply(MacroPlayer& mp, const CameraPreset& p, bool reset_to_zero_first = true);

} // namespace bonez::plugins
