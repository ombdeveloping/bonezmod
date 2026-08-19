#pragma once
#include "vision/detector.h"
#include "overlay/overlay.h"

#include <deque>

namespace bonez::plugins {

// Buffer of recent ball detections, drawn as a fading polyline. Since we
// only get position (not velocity) from vision, the "trail" is the last N
// observed positions rather than a physics prediction.
class BallTrail {
public:
    void configure(size_t max_samples = 40, float fade = 0.94f) {
        max_ = max_samples; fade_ = fade;
    }
    void push(const FrameFeatures& f);
    void draw(Overlay::PaintCtx& c, const RECT& game_rect_screen) const;

private:
    struct Sample { float x, y, r; };
    std::deque<Sample> pts_;
    size_t max_  = 40;
    float  fade_ = 0.94f;
};

} // namespace bonez::plugins
