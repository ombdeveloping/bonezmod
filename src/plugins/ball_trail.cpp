#include "plugins/ball_trail.h"

#include <wrl/client.h>
#include <d2d1_1.h>

namespace bonez::plugins {

void BallTrail::push(const FrameFeatures& f) {
    if (!f.ball) return;
    pts_.push_back({ f.ball->x, f.ball->y, f.ball->radius });
    while (pts_.size() > max_) pts_.pop_front();
}

void BallTrail::draw(Overlay::PaintCtx& c, const RECT& r) const {
    if (pts_.size() < 2) return;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.6f, 0.1f, 1.0f),
                                 brush.GetAddressOf());
    float alpha = 1.0f;
    for (size_t i = pts_.size(); i > 1; --i) {
        alpha *= fade_;
        const auto& a = pts_[i - 1];
        const auto& b = pts_[i - 2];
        brush->SetColor(D2D1::ColorF(1.0f, 0.6f, 0.1f, alpha));
        c.d2d->DrawLine(
            D2D1::Point2F(a.x - r.left, a.y - r.top),
            D2D1::Point2F(b.x - r.left, b.y - r.top),
            brush.Get(), 3.0f);
    }
}

} // namespace bonez::plugins
