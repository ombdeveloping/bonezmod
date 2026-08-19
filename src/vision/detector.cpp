#include "vision/detector.h"

#if BONEZ_HAS_OPENCV
  #include <opencv2/opencv.hpp>
#endif

#include <algorithm>

namespace bonez {

#if BONEZ_HAS_OPENCV

static cv::Mat wrap_bgra(const DxgiCapture::Frame& f) {
    // Wraps the captured BGRA buffer with the correct stride, no copy.
    return cv::Mat(f.h, f.w, CV_8UC4,
                   const_cast<uint8_t*>(f.bgra.data()),
                   (size_t)f.stride);
}

static cv::Mat crop_to_game(const cv::Mat& full, const RECT& r) {
    int x = std::max(0, (int)r.left);
    int y = std::max(0, (int)r.top);
    int w = std::min(full.cols - x, (int)(r.right - r.left));
    int h = std::min(full.rows - y, (int)(r.bottom - r.top));
    if (w <= 0 || h <= 0) return cv::Mat();
    return full(cv::Rect(x, y, w, h)).clone();
}

static std::optional<BallEstimate> detect_ball(const cv::Mat& game_bgra) {
    cv::Mat gray;
    cv::cvtColor(game_bgra, gray, cv::COLOR_BGRA2GRAY);
    cv::medianBlur(gray, gray, 5);

    std::vector<cv::Vec3f> circles;
    const int min_r = std::max(8,  game_bgra.rows / 90);
    const int max_r = std::max(20, game_bgra.rows / 25);
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1.2,
                     (double)game_bgra.rows / 8.0,
                     140.0, 30.0, min_r, max_r);
    if (circles.empty()) return std::nullopt;

    // Pick highest-y (usually mid-air ball) with strongest gradient response.
    const auto& c = circles.front();
    BallEstimate b;
    b.x = c[0]; b.y = c[1]; b.radius = c[2];
    b.confidence = std::min(1.0f, (float)circles.size() / 4.0f + 0.4f);
    return b;
}

static std::optional<BoostEstimate> detect_boost(const cv::Mat& game_bgra) {
    // Boost meter sits bottom-right; sample its bar fill ratio via saturation.
    const int W = game_bgra.cols, H = game_bgra.rows;
    cv::Rect roi(W - W/6, H - H/5, W/8, H/12);
    roi &= cv::Rect(0, 0, W, H);
    if (roi.area() <= 0) return std::nullopt;

    cv::Mat sub = game_bgra(roi);
    cv::Mat hsv;
    cv::cvtColor(sub, hsv, cv::COLOR_BGRA2BGR);
    cv::cvtColor(hsv, hsv, cv::COLOR_BGR2HSV);

    // Bright orange/yellow mask (boost fill).
    cv::Mat mask;
    cv::inRange(hsv, cv::Scalar(10, 120, 150), cv::Scalar(35, 255, 255), mask);
    double filled = cv::countNonZero(mask);
    double total  = mask.rows * mask.cols;
    if (total <= 0) return std::nullopt;

    BoostEstimate be;
    be.value      = (int)std::round(100.0 * filled / total);
    be.confidence = 0.6f;
    return be;
}

FrameFeatures analyze(const DxgiCapture::Frame& f, const RECT& r) {
    FrameFeatures out;
    if (f.bgra.empty() || f.w <= 0 || f.h <= 0) return out;
    cv::Mat full = wrap_bgra(f);
    cv::Mat game = crop_to_game(full, r);
    if (game.empty()) return out;

    out.ball  = detect_ball(game);
    out.boost = detect_boost(game);

    // Shift ball back to screen coords for the overlay.
    if (out.ball) {
        out.ball->x += r.left;
        out.ball->y += r.top;
    }
    return out;
}

#else // no OpenCV

FrameFeatures analyze(const DxgiCapture::Frame&, const RECT&) {
    return {};
}

#endif

} // namespace bonez
