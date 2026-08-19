#include "overlay/rank_badge.h"

#include <shlobj.h>
#include <knownfolders.h>
#include <algorithm>
#include <cctype>
#include <cmath>

using Microsoft::WRL::ComPtr;

namespace bonez {

namespace {

std::string tier_family(const std::string& tier) {
    std::string t = tier;
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    if (t.find("supersonic")     != std::string::npos) return "supersonic_legend";
    if (t.find("grand champion") != std::string::npos) return "grand_champion";
    if (t.find("champion")       != std::string::npos) return "champion";
    if (t.find("diamond")        != std::string::npos) return "diamond";
    if (t.find("platinum")       != std::string::npos) return "platinum";
    if (t.find("gold")           != std::string::npos) return "gold";
    if (t.find("silver")         != std::string::npos) return "silver";
    if (t.find("bronze")         != std::string::npos) return "bronze";
    return "unranked";
}

// Tier -> accent (kept in sync with mmr_reveal's inline colors).
D2D1::ColorF tier_color(const std::string& family) {
    if (family == "supersonic_legend") return D2D1::ColorF(1.00f, 0.35f, 0.90f);
    if (family == "grand_champion")    return D2D1::ColorF(0.75f, 0.45f, 1.00f);
    if (family == "champion")          return D2D1::ColorF(0.55f, 0.70f, 1.00f);
    if (family == "diamond")           return D2D1::ColorF(0.35f, 0.85f, 1.00f);
    if (family == "platinum")          return D2D1::ColorF(0.55f, 1.00f, 0.85f);
    if (family == "gold")              return D2D1::ColorF(1.00f, 0.85f, 0.30f);
    if (family == "silver")            return D2D1::ColorF(0.85f, 0.85f, 0.90f);
    if (family == "bronze")            return D2D1::ColorF(0.85f, 0.55f, 0.35f);
    return D2D1::ColorF(1, 1, 1, 0.6f);
}

std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// Roman numeral for tier level 1..3 (Bronze I, II, III...).
std::wstring roman_from_tier(const std::string& tier) {
    std::string t = tier;
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    if (t.find(" iii") != std::string::npos) return L"III";
    if (t.find(" ii")  != std::string::npos) return L"II";
    if (t.find(" i")   != std::string::npos) return L"I";
    return L"";
}

// A tiny path-geometry builder to keep silhouettes readable.
struct PathBuilder {
    ComPtr<ID2D1PathGeometry> path;
    ComPtr<ID2D1GeometrySink> sink;
    PathBuilder(ID2D1DeviceContext* d2d) {
        ComPtr<ID2D1Factory> f;
        d2d->GetFactory(f.GetAddressOf());
        f->CreatePathGeometry(path.GetAddressOf());
        path->Open(sink.GetAddressOf());
    }
    void begin(float x, float y) {
        sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
    }
    void line(float x, float y) { sink->AddLine(D2D1::Point2F(x, y)); }
    void end() { sink->EndFigure(D2D1_FIGURE_END_CLOSED); }
    ComPtr<ID2D1PathGeometry> finish() { sink->Close(); return path; }
};

// Each tier is drawn as one closed path so fill + stroke work together.
namespace shapes {

ComPtr<ID2D1PathGeometry> bronze(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Circle with two vertical bars carved out (rendered as text stroke).
    ComPtr<ID2D1Factory> f; d->GetFactory(f.GetAddressOf());
    ComPtr<ID2D1EllipseGeometry> e;
    f->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r),
                             e.GetAddressOf());
    // Wrap in a path geometry via outline.
    ComPtr<ID2D1PathGeometry> p;
    f->CreatePathGeometry(p.GetAddressOf());
    ComPtr<ID2D1GeometrySink> s;
    p->Open(s.GetAddressOf());
    e->Outline(nullptr, s.Get());
    s->Close();
    return p;
}

ComPtr<ID2D1PathGeometry> silver(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Regular hexagon, flat top.
    PathBuilder pb(d);
    const float a = r;
    const float b = a * 0.866f;
    pb.begin(cx - a, cy);
    pb.line(cx - a * 0.5f, cy - b);
    pb.line(cx + a * 0.5f, cy - b);
    pb.line(cx + a,        cy);
    pb.line(cx + a * 0.5f, cy + b);
    pb.line(cx - a * 0.5f, cy + b);
    pb.end();
    return pb.finish();
}

ComPtr<ID2D1PathGeometry> gold(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Downward-pointing chevron crest: three stacked V shapes fused.
    PathBuilder pb(d);
    pb.begin(cx - r,        cy - r * 0.7f);
    pb.line(cx,             cy - r * 0.9f);
    pb.line(cx + r,         cy - r * 0.7f);
    pb.line(cx + r * 0.75f, cy - r * 0.3f);
    pb.line(cx + r * 0.55f, cy - r * 0.5f);
    pb.line(cx,             cy - r * 0.15f);
    pb.line(cx - r * 0.55f, cy - r * 0.5f);
    pb.line(cx - r * 0.75f, cy - r * 0.3f);
    pb.end();

    // Second chevron closes the badge silhouette below; draw as a separate
    // subpath in the same geometry.
    ComPtr<ID2D1Factory> f; d->GetFactory(f.GetAddressOf());
    ComPtr<ID2D1PathGeometry> lower; f->CreatePathGeometry(lower.GetAddressOf());
    ComPtr<ID2D1GeometrySink> s; lower->Open(s.GetAddressOf());
    s->BeginFigure(D2D1::Point2F(cx - r * 0.8f, cy - r * 0.1f),
                   D2D1_FIGURE_BEGIN_FILLED);
    s->AddLine(D2D1::Point2F(cx,                cy + r * 0.4f));
    s->AddLine(D2D1::Point2F(cx + r * 0.8f,     cy - r * 0.1f));
    s->AddLine(D2D1::Point2F(cx + r * 0.55f,    cy + r * 0.15f));
    s->AddLine(D2D1::Point2F(cx,                cy + r * 0.75f));
    s->AddLine(D2D1::Point2F(cx - r * 0.55f,    cy + r * 0.15f));
    s->EndFigure(D2D1_FIGURE_END_CLOSED);
    s->Close();

    ComPtr<ID2D1GeometryGroup> group;
    ID2D1Geometry* parts[2] = { pb.finish().Get(), lower.Get() };
    f->CreateGeometryGroup(D2D1_FILL_MODE_WINDING, parts, 2, group.GetAddressOf());
    ComPtr<ID2D1PathGeometry> out; f->CreatePathGeometry(out.GetAddressOf());
    ComPtr<ID2D1GeometrySink> os; out->Open(os.GetAddressOf());
    group->Outline(nullptr, os.Get());
    os->Close();
    return out;
}

ComPtr<ID2D1PathGeometry> platinum(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Five-point star.
    PathBuilder pb(d);
    const float outer = r;
    const float inner = r * 0.44f;
    const float PI = 3.14159265f;
    for (int i = 0; i < 10; ++i) {
        float rr = (i & 1) ? inner : outer;
        float a  = -PI / 2 + i * (PI / 5);
        float x = cx + rr * std::cos(a);
        float y = cy + rr * std::sin(a);
        if (i == 0) pb.begin(x, y); else pb.line(x, y);
    }
    pb.end();
    return pb.finish();
}

ComPtr<ID2D1PathGeometry> diamond(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Rhombus with slanted top edges (RL's diamond gem silhouette).
    PathBuilder pb(d);
    pb.begin(cx, cy - r);
    pb.line(cx + r,        cy - r * 0.30f);
    pb.line(cx + r * 0.75f,cy - r * 0.55f);
    pb.line(cx + r,        cy - r * 0.30f);
    pb.line(cx,            cy + r);
    pb.line(cx - r,        cy - r * 0.30f);
    pb.line(cx - r * 0.75f,cy - r * 0.55f);
    pb.line(cx - r,        cy - r * 0.30f);
    pb.end();
    return pb.finish();
}

ComPtr<ID2D1PathGeometry> champion(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Diamond with two rising horns / stylized crown.
    PathBuilder pb(d);
    pb.begin(cx,                cy + r);
    pb.line(cx + r,             cy);
    pb.line(cx + r * 0.55f,     cy - r * 0.30f);
    pb.line(cx + r * 0.85f,     cy - r * 0.90f);
    pb.line(cx + r * 0.30f,     cy - r * 0.55f);
    pb.line(cx,                 cy - r);
    pb.line(cx - r * 0.30f,     cy - r * 0.55f);
    pb.line(cx - r * 0.85f,     cy - r * 0.90f);
    pb.line(cx - r * 0.55f,     cy - r * 0.30f);
    pb.line(cx - r,             cy);
    pb.end();
    return pb.finish();
}

ComPtr<ID2D1PathGeometry> grand_champion(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Winged crest with a central gem.
    PathBuilder pb(d);
    // Left wing
    pb.begin(cx - r * 1.10f, cy - r * 0.10f);
    pb.line(cx - r * 0.85f, cy - r * 0.55f);
    pb.line(cx - r * 0.55f, cy - r * 0.20f);
    pb.line(cx - r * 0.75f, cy - r * 0.35f);
    pb.line(cx - r * 0.35f, cy - r * 0.10f);
    // Gem top
    pb.line(cx,             cy - r * 0.85f);
    // Right side (mirror)
    pb.line(cx + r * 0.35f, cy - r * 0.10f);
    pb.line(cx + r * 0.75f, cy - r * 0.35f);
    pb.line(cx + r * 0.55f, cy - r * 0.20f);
    pb.line(cx + r * 0.85f, cy - r * 0.55f);
    pb.line(cx + r * 1.10f, cy - r * 0.10f);
    // Bottom point
    pb.line(cx + r * 0.55f, cy + r * 0.10f);
    pb.line(cx,             cy + r * 0.75f);
    pb.line(cx - r * 0.55f, cy + r * 0.10f);
    pb.end();
    return pb.finish();
}

ComPtr<ID2D1PathGeometry> ssl(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Winged spear/dart with center prism (bolder wings than GC).
    PathBuilder pb(d);
    pb.begin(cx - r * 1.20f, cy);
    pb.line(cx - r * 0.90f, cy - r * 0.65f);
    pb.line(cx - r * 0.30f, cy - r * 0.20f);
    pb.line(cx,             cy - r);
    pb.line(cx + r * 0.30f, cy - r * 0.20f);
    pb.line(cx + r * 0.90f, cy - r * 0.65f);
    pb.line(cx + r * 1.20f, cy);
    pb.line(cx + r * 0.55f, cy + r * 0.20f);
    pb.line(cx,             cy + r * 0.95f);
    pb.line(cx - r * 0.55f, cy + r * 0.20f);
    pb.end();
    return pb.finish();
}

ComPtr<ID2D1PathGeometry> unranked(ID2D1DeviceContext* d, float cx, float cy, float r) {
    // Plain shield.
    PathBuilder pb(d);
    pb.begin(cx - r,  cy - r);
    pb.line(cx + r,   cy - r);
    pb.line(cx + r,   cy + r * 0.3f);
    pb.line(cx,       cy + r);
    pb.line(cx - r,   cy + r * 0.3f);
    pb.end();
    return pb.finish();
}

} // namespace shapes

} // namespace

void RankBadge::draw_procedural(ID2D1DeviceContext* d2d, IDWriteFactory* dw,
                                const std::string& fam,
                                const std::string& full_tier,
                                const D2D1_RECT_F& dst) {
    auto col = tier_color(fam);
    ComPtr<ID2D1SolidColorBrush> fill, stroke, ink;
    d2d->CreateSolidColorBrush(D2D1::ColorF(col.r, col.g, col.b, 0.28f),
                               fill.GetAddressOf());
    d2d->CreateSolidColorBrush(col, stroke.GetAddressOf());
    d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.95f),
                               ink.GetAddressOf());

    float cx = (dst.left + dst.right) * 0.5f;
    float cy = (dst.top + dst.bottom) * 0.5f;
    float r  = std::min(dst.right - dst.left, dst.bottom - dst.top) * 0.42f;

    ComPtr<ID2D1PathGeometry> g;
    if      (fam == "bronze")             g = shapes::bronze(d2d, cx, cy, r);
    else if (fam == "silver")             g = shapes::silver(d2d, cx, cy, r);
    else if (fam == "gold")               g = shapes::gold(d2d, cx, cy, r);
    else if (fam == "platinum")           g = shapes::platinum(d2d, cx, cy, r);
    else if (fam == "diamond")            g = shapes::diamond(d2d, cx, cy, r);
    else if (fam == "champion")           g = shapes::champion(d2d, cx, cy, r);
    else if (fam == "grand_champion")     g = shapes::grand_champion(d2d, cx, cy, r);
    else if (fam == "supersonic_legend")  g = shapes::ssl(d2d, cx, cy, r);
    else                                  g = shapes::unranked(d2d, cx, cy, r);

    d2d->FillGeometry(g.Get(), fill.Get());
    d2d->DrawGeometry(g.Get(), stroke.Get(), 1.6f);

    // Roman numeral (I / II / III) small in the center for the four
    // shapes that read cleanly with text overlaid.
    std::wstring rn = roman_from_tier(full_tier);
    if (!rn.empty() &&
        (fam == "bronze" || fam == "silver" || fam == "gold" ||
         fam == "diamond")) {
        ComPtr<IDWriteTextFormat> fmt;
        dw->CreateTextFormat(L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            r * 0.55f, L"en-us", fmt.GetAddressOf());
        fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        D2D1_RECT_F tr = D2D1::RectF(dst.left, dst.top,
                                      dst.right, dst.bottom);
        d2d->DrawTextW(rn.c_str(), (UINT32)rn.size(),
                       fmt.Get(), tr, ink.Get());
    }
}

bool RankBadge::ensure_wic() {
    if (wic_) return true;
    // WIC needs COM initialized on this thread.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_)));
}

ComPtr<ID2D1Bitmap>
RankBadge::load_png(ID2D1DeviceContext* d2d, const std::string& family) {
    if (dirs_.empty()) return {};
    if (!ensure_wic()) return {};

    ComPtr<IWICBitmapDecoder> dec;
    for (const auto& dir : dirs_) {
        if (dir.empty()) continue;
        std::wstring path = dir + L"\\" + to_wide(family) + L".png";
        if (SUCCEEDED(wic_->CreateDecoderFromFilename(path.c_str(), nullptr,
            GENERIC_READ, WICDecodeMetadataCacheOnLoad,
            dec.ReleaseAndGetAddressOf()))) break;
    }
    if (!dec) return {};
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(dec->GetFrame(0, frame.GetAddressOf()))) return {};

    ComPtr<IWICFormatConverter> conv;
    if (FAILED(wic_->CreateFormatConverter(conv.GetAddressOf()))) return {};
    if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return {};

    ComPtr<ID2D1Bitmap> bmp;
    if (FAILED(d2d->CreateBitmapFromWicBitmap(conv.Get(), nullptr,
        bmp.GetAddressOf())))
        return {};
    return bmp;
}

void RankBadge::draw(ID2D1DeviceContext* d2d, IDWriteFactory* dw,
                     const std::string& tier, int division,
                     const D2D1_RECT_F& dst) {
    std::string fam = tier_family(tier);

    // Try to serve a real PNG.
    ID2D1Bitmap* png = nullptr;
    auto it = cache_.find(fam);
    if (it == cache_.end()) {
        if (!tried_[fam]) {
            tried_[fam] = true;
            auto bmp = load_png(d2d, fam);
            if (bmp) { cache_[fam] = bmp; png = bmp.Get(); }
        }
    } else {
        png = it->second.Get();
    }

    if (png) {
        d2d->DrawBitmap(png, dst, 1.0f,
            D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    } else {
        draw_procedural(d2d, dw, fam, tier, dst);
    }

    // Division dots below the badge, 1..4 always drawn (filled if <= div).
    if (division > 0) {
        ComPtr<ID2D1SolidColorBrush> filled, empty;
        auto col = tier_color(fam);
        d2d->CreateSolidColorBrush(col, filled.GetAddressOf());
        d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.25f),
                                   empty.GetAddressOf());
        float bot = dst.bottom + 2;
        float cx = (dst.left + dst.right) * 0.5f;
        float sp = 6;
        for (int i = 1; i <= 4; ++i) {
            float x = cx + (i - 2.5f) * sp;
            D2D1_ELLIPSE d{ D2D1::Point2F(x, bot + 3), 2.0f, 2.0f };
            d2d->FillEllipse(d, i <= division ? filled.Get() : empty.Get());
        }
    }
}

} // namespace bonez
