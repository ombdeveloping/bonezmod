#include "overlay/overlay.h"

#include <dcomp.h>
#include <dwmapi.h>
#include <dxgi1_3.h>

using Microsoft::WRL::ComPtr;

namespace bonez {

static const wchar_t* kClassName = L"BonezmodOverlayCls";

LRESULT CALLBACK Overlay::wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

bool Overlay::init(const RECT& r) {
    rect_ = r;
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance   = GetModuleHandleW(nullptr);
    wc.hCursor     = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    DWORD ex = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT
             | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP;
    hwnd_ = CreateWindowExW(ex, kClassName, L"bonezmod",
                            WS_POPUP,
                            r.left, r.top,
                            r.right - r.left, r.bottom - r.top,
                            nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd_) return false;
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, nullptr, 0, D3D11_SDK_VERSION,
        dev_.GetAddressOf(), &fl, ctx_.GetAddressOf()))) return false;

    ComPtr<IDXGIDevice> dxdev;
    dev_.As(&dxdev);
    ComPtr<IDXGIFactory2> fac;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&fac));

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width  = (UINT)(r.right - r.left);
    sd.Height = (UINT)(r.bottom - r.top);
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;
    sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (FAILED(fac->CreateSwapChainForComposition(dxdev.Get(), &sd, nullptr,
                                                  swap_.GetAddressOf())))
        return false;

    if (FAILED(DCompositionCreateDevice(dxdev.Get(), IID_PPV_ARGS(&dcomp_))))
        return false;
    if (FAILED(dcomp_->CreateTargetForHwnd(hwnd_, TRUE, dcomp_target_.GetAddressOf())))
        return false;
    if (FAILED(dcomp_->CreateVisual(dcomp_visual_.GetAddressOf())))
        return false;
    dcomp_visual_->SetContent(swap_.Get());
    dcomp_target_->SetRoot(dcomp_visual_.Get());
    dcomp_->Commit();

    D2D1_FACTORY_OPTIONS o{};
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                      __uuidof(ID2D1Factory1), &o,
                      (void**)d2d_factory_.GetAddressOf());
    d2d_factory_->CreateDevice(dxdev.Get(), d2d_dev_.GetAddressOf());
    d2d_dev_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                  d2d_ctx_.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        (IUnknown**)dw_.GetAddressOf());
    return make_target();
}

bool Overlay::make_target() {
    d2d_ctx_->SetTarget(nullptr);
    d2d_target_.Reset();
    ComPtr<IDXGISurface> surf;
    if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(&surf)))) return false;
    D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(d2d_ctx_->CreateBitmapFromDxgiSurface(surf.Get(), &bp,
                                                     d2d_target_.GetAddressOf())))
        return false;
    d2d_ctx_->SetTarget(d2d_target_.Get());
    return true;
}

void Overlay::shutdown() {
    d2d_target_.Reset(); d2d_ctx_.Reset(); d2d_dev_.Reset(); d2d_factory_.Reset();
    dcomp_visual_.Reset(); dcomp_target_.Reset(); dcomp_.Reset();
    swap_.Reset(); ctx_.Reset(); dev_.Reset();
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
}

void Overlay::reposition(const RECT& r) {
    if (!hwnd_) return;
    if (memcmp(&r, &rect_, sizeof(RECT)) == 0) return;
    rect_ = r;
    SetWindowPos(hwnd_, HWND_TOPMOST, r.left, r.top,
                 r.right - r.left, r.bottom - r.top,
                 SWP_NOACTIVATE);
    d2d_ctx_->SetTarget(nullptr);
    d2d_target_.Reset();
    swap_->ResizeBuffers(0, (UINT)(r.right - r.left), (UINT)(r.bottom - r.top),
                         DXGI_FORMAT_UNKNOWN, 0);
    make_target();
}

void Overlay::set_visible(bool on) {
    if (!hwnd_) return;
    ShowWindow(hwnd_, on ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void Overlay::set_click_through(bool on) {
    if (!hwnd_) return;
    LONG ex = GetWindowLongW(hwnd_, GWL_EXSTYLE);
    if (on) ex |=  WS_EX_TRANSPARENT;
    else    ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongW(hwnd_, GWL_EXSTYLE, ex);
}

bool Overlay::pump_and_render() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    d2d_ctx_->BeginDraw();
    d2d_ctx_->Clear(D2D1::ColorF(0, 0, 0, 0));
    if (paint_) {
        PaintCtx pc{ d2d_ctx_.Get(), dw_.Get(),
                     rect_.right - rect_.left, rect_.bottom - rect_.top };
        paint_(pc);
    }
    d2d_ctx_->EndDraw();
    swap_->Present(1, 0);
    return true;
}

// -----------------------------------------------------------------------

static void draw_text(Overlay::PaintCtx& c, const std::wstring& s,
                      float x, float y, float size, D2D1::ColorF col) {
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmt;
    c.dw->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", fmt.GetAddressOf());
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    c.d2d->CreateSolidColorBrush(col, brush.GetAddressOf());
    D2D1_RECT_F r = D2D1::RectF(x, y, x + 1200, y + size * 1.4f);
    c.d2d->DrawTextW(s.c_str(), (UINT32)s.size(), fmt.Get(), r, brush.Get());
}

void paint_hud(Overlay::PaintCtx& c, const HudState& s) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panel;
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0.45f),
                                 panel.GetAddressOf());
    D2D1_ROUNDED_RECT rr{ D2D1::RectF(16, 16, 460, 128), 10, 10 };
    c.d2d->FillRoundedRectangle(rr, panel.Get());

    draw_text(c, L"bonezmod", 28, 22, 20,
              D2D1::ColorF(0.85f, 0.95f, 1.0f, 1.0f));
    draw_text(c, s.status_line, 28, 52, 14,
              D2D1::ColorF(1, 1, 1, 0.9f));
    draw_text(c, s.rank_line, 28, 74, 14,
              D2D1::ColorF(1, 1, 1, 0.9f));

    if (s.boost >= 0) {
        std::wstring b = L"boost: " + std::to_wstring(s.boost);
        draw_text(c, b, 28, 96, 14, D2D1::ColorF(1.0f, 0.85f, 0.3f, 1.0f));
    }

    if (s.eac_warning) {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> red;
        c.d2d->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.1f, 0.1f, 0.85f),
                                     red.GetAddressOf());
        D2D1_ROUNDED_RECT bar{ D2D1::RectF(16, (float)c.h - 56,
                                           (float)c.w - 16, (float)c.h - 16), 8, 8 };
        c.d2d->FillRoundedRectangle(bar, red.Get());
        draw_text(c,
            L"EAC ACTIVE — pad + macros disarmed. no injection, no reads.",
            32, (float)c.h - 48, 16, D2D1::ColorF(1, 1, 1, 1));
    }

    if (s.ball_x > 0 && s.ball_y > 0 && s.ball_r > 0) {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> ring;
        c.d2d->CreateSolidColorBrush(D2D1::ColorF(0.2f, 1.0f, 0.4f, 0.85f),
                                     ring.GetAddressOf());
        D2D1_ELLIPSE e{ D2D1::Point2F(s.ball_x, s.ball_y),
                        s.ball_r + 4, s.ball_r + 4 };
        c.d2d->DrawEllipse(e, ring.Get(), 2.5f);
    }
}

} // namespace bonez
