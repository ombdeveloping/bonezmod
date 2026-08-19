#pragma once
#include "capture/dxgi_capture.h"
#include <optional>

namespace bonez {

struct BallEstimate {
    float x = 0, y = 0;    // screen pixels
    float radius = 0;
    float confidence = 0;  // 0..1
};

struct BoostEstimate {
    int   value = 0;        // 0..100
    float confidence = 0;
};

struct FrameFeatures {
    std::optional<BallEstimate>  ball;
    std::optional<BoostEstimate> boost;
    bool goal_replay = false;    // "GOAL" banner detected
    bool in_menu     = false;
};

// Analyze one captured frame. When OpenCV is present, uses HoughCircles for
// the ball and a color-mask + digit ROI for boost. Without OpenCV, returns
// an empty feature set (never fabricates data).
FrameFeatures analyze(const DxgiCapture::Frame& f, const RECT& game_rect_screen);

} // namespace bonez
