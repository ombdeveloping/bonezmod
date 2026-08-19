#pragma once
#include "capture/dxgi_capture.h"
#include "input/macro.h"

namespace bonez::plugins {

// Watches the captured framebuffer for the "Match Found — press A to
// accept" prompt. When it fires the callback taps A on the virtual pad.
// Pure pixel-based signal; no game-process contact.
//
// The prompt sits center-screen with a distinct bright-orange border. We
// sample a ring of pixels in that ROI and match the orange dominance.
class AutoQueue {
public:
    void configure(int game_w, int game_h);
    // Returns true if the prompt is currently visible in the frame.
    bool prompt_visible(const DxgiCapture::Frame& f, const RECT& game_rect);
    // Ready-up if visible and cooldown expired. Uses `pad` via `mp`.
    void tick(const DxgiCapture::Frame& f, const RECT& r, MacroPlayer& mp);

private:
    int cx_ = 0, cy_ = 0, rad_ = 0;
    uint64_t last_press_seq_ = 0;
};

// After a completed match, drives the menu back to the play button.
void requeue(MacroPlayer& mp);

} // namespace bonez::plugins
