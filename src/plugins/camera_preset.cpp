#include "plugins/camera_preset.h"

namespace bonez::plugins {

const CameraPreset kSquishy { 110, 110,  -3, 270,  50, 250, 100, true, false };
const CameraPreset kJstn    { 109, 110,  -3, 270,  50, 260, 140, true, false };
const CameraPreset kRizzo   { 110, 100,  -4, 270,  50, 250, 100, true, false };

namespace {
using ms = std::chrono::milliseconds;
Step tap(PadState p, int hold = 70) { return { p, ms(hold) }; }
Step wait(int h) { return { PadState{}, ms(h) }; }

void push_taps(std::vector<Step>& v, const PadState& p, int n, int gap = 40) {
    for (int i = 0; i < n; ++i) {
        v.push_back(tap(p, 60));
        v.push_back(wait(gap));
    }
}

// Each RL camera slider ranges in known integer steps; one dpad-left/right
// tap moves it by 1. Reset by holding LEFT for `max_span` taps, then step
// right by the delta to the target.
void set_slider(std::vector<Step>& v, int target, int lo, int hi,
                bool reset_first) {
    PadState L; L.dpad_left  = true;
    PadState R; R.dpad_right = true;
    if (reset_first) push_taps(v, L, (hi - lo) + 2);
    push_taps(v, R, target - lo);
    // Move to next slider (down).
    PadState D; D.dpad_down = true;
    v.push_back(tap(D, 70)); v.push_back(wait(60));
}
} // namespace

void apply(MacroPlayer& mp, const CameraPreset& p, bool reset_first) {
    std::vector<Step> s;
    // Enter pause → Settings.
    PadState start; start.start = true;
    PadState down;  down.dpad_down = true;
    PadState right; right.dpad_right = true;
    PadState A;     A.a = true;

    s.push_back(tap(start, 90)); s.push_back(wait(400));
    // Down 3 taps to "Options"
    push_taps(s, down, 3);
    s.push_back(tap(A, 90));     s.push_back(wait(500));
    // Right to "Camera" tab.
    push_taps(s, right, 3);
    s.push_back(wait(200));

    // Camera Shake off, ball cam toggle on: skip; user handles those.
    // Sliders in this order in-game: FOV, Height, Angle, Distance,
    // Stiffness, Swivel Speed, Transition Speed.
    set_slider(s, p.fov,        60,  110, reset_first);
    set_slider(s, p.height,     40,  200, reset_first);
    set_slider(s, p.angle,      -7,    0, reset_first);
    set_slider(s, p.distance,  120,  270, reset_first);
    set_slider(s, p.stiffness,   0,  100, reset_first);
    set_slider(s, p.swivel,    100, 1000, reset_first);
    set_slider(s, p.transition,  0,  200, reset_first);

    // Back out twice.
    PadState B; B.b = true;
    s.push_back(tap(B, 90)); s.push_back(wait(300));
    s.push_back(tap(B, 90));

    mp.play(std::move(s));
}

} // namespace bonez::plugins
