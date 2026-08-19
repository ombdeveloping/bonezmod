#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <functional>
#include <string>

namespace bonez {

// Standalone, click-through, per-pixel-alpha overlay window that lives on
// top of the game window. It never touches RL — it just repaints itself
// above it using DirectComposition. Painting is done via a caller-provided
// callback each frame.
class Overlay {
public:
    struct PaintCtx {
        ID2D1DeviceContext*  d2d = nullptr;
        IDWriteFactory*      dw  = nullptr;
        int w = 0, h = 0;
    };

    using PaintFn = std::function<void(PaintCtx&)>;

    bool init(const RECT& screen_rect);
    void shutdown();
    void reposition(const RECT& screen_rect);
    void set_paint(PaintFn fn) { paint_ = std::move(fn); }
    void set_click_through(bool on);
    void set_visible(bool on);
    HWND hwnd() const { return hwnd_; }

    // Pump one frame: process messages, redraw.
    bool pump_and_render();

private:
    static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);

    HWND hwnd_ = nullptr;
    PaintFn paint_;
    RECT rect_{};

    Microsoft::WRL::ComPtr<ID3D11Device>         dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>  ctx_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>      swap_;
    Microsoft::WRL::ComPtr<IDCompositionDevice>  dcomp_;
    Microsoft::WRL::ComPtr<IDCompositionTarget>  dcomp_target_;
    Microsoft::WRL::ComPtr<IDCompositionVisual>  dcomp_visual_;
    Microsoft::WRL::ComPtr<ID2D1Factory1>        d2d_factory_;
    Microsoft::WRL::ComPtr<ID2D1Device>          d2d_dev_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext>   d2d_ctx_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1>         d2d_target_;
    Microsoft::WRL::ComPtr<IDWriteFactory>       dw_;

    bool make_target();
};

// Convenience painters --------------------------------------------------
struct HudState {
    bool eac_warning = false;
    std::wstring status_line;
    std::wstring rank_line;
    int  boost = -1;                // -1 = unknown
    float ball_x = -1, ball_y = -1; // -1 = unknown
    float ball_r = 0;
};

void paint_hud(Overlay::PaintCtx& c, const HudState& s);

} // namespace bonez
