#pragma once
#include "vision/detector.h"

namespace bonez::plugins {

// Given a ball detection in screen coords + the game rect, compute the
// mirrored point across the field's vertical center line. Useful for a
// left/right-symmetry training overlay marker.
struct MirrorPoint { float x = 0, y = 0; bool valid = false; };
MirrorPoint mirror_ball(const FrameFeatures& f, const RECT& game_rect);

} // namespace bonez::plugins
