#include "plugins/auto_queue.h"

#include <algorithm>
#include <cmath>

namespace bonez::plugins {

namespace {
using ms = std::chrono::milliseconds;
Step tap(PadState p, int hold = 90) { return { p, ms(hold) }; }
Step wait(int h) { return { PadState{}, ms(h) }; }

bool is_orange(uint8_t b, uint8_t g, uint8_t r) {
    // Rocket League's accept-prompt orange sits around (255, 145, 40) in BGR
    // form that's (40, 145, 255). Allow generous slack.
    return r > 200 && g > 100 && g < 190 && b < 90;
}
} // namespace

void AutoQueue::configure(int w, int h) {
    cx_ = w / 2;
    cy_ = (int)(h * 0.55f);   // prompt sits slightly below center
    rad_ = std::min(w, h) / 10;
}

bool AutoQueue::prompt_visible(const DxgiCapture::Frame& f, const RECT& r) {
    if (f.bgra.empty()) return false;
    int gw = r.right - r.left;
    int gh = r.bottom - r.top;
    if (gw <= 0 || gh <= 0) return false;
    if (cx_ == 0) configure(gw, gh);

    int ox = r.left + cx_;
    int oy = r.top  + cy_;
    // Sample 32 points on a ring at `rad_` around the ROI center.
    int hits = 0, samples = 32;
    for (int i = 0; i < samples; ++i) {
        double a = (i / (double)samples) * 6.28318;
        int px = ox + (int)std::lround(std::cos(a) * rad_);
        int py = oy + (int)std::lround(std::sin(a) * rad_);
        if (px < 0 || py < 0 || px >= f.w || py >= f.h) continue;
        const uint8_t* p = f.bgra.data() + (size_t)py * f.stride + (size_t)px * 4;
        if (is_orange(p[0], p[1], p[2])) ++hits;
    }
    // >40% of the ring reading as accent-orange = prompt visible.
    return hits > samples * 0.4;
}

void AutoQueue::tick(const DxgiCapture::Frame& f, const RECT& r, MacroPlayer& mp) {
    if (f.seq == last_press_seq_) return;
    if (!prompt_visible(f, r)) return;
    last_press_seq_ = f.seq;
    PadState a; a.a = true;
    mp.play({ tap(a, 100), wait(120), tap(a, 100) });
}

void requeue(MacroPlayer& mp) {
    // At the end-of-match summary: A dismisses, then Play → same playlist.
    std::vector<Step> s;
    PadState a; a.a = true;
    s.push_back(tap(a, 100)); s.push_back(wait(600));
    s.push_back(tap(a, 100)); s.push_back(wait(400));
    s.push_back(tap(a, 100)); s.push_back(wait(400));
    // Play → previous playlist highlighted → confirm.
    s.push_back(tap(a, 100));
    mp.play(std::move(s));
}

} // namespace bonez::plugins
