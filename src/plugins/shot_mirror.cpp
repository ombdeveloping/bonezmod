#include "plugins/shot_mirror.h"

namespace bonez::plugins {

MirrorPoint mirror_ball(const FrameFeatures& f, const RECT& r) {
    MirrorPoint m;
    if (!f.ball) return m;
    float cx = (r.left + r.right) * 0.5f;
    m.x = 2.0f * cx - f.ball->x;
    m.y = f.ball->y;
    m.valid = true;
    return m;
}

} // namespace bonez::plugins
