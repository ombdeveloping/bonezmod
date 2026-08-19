#include "overlay/rank_badge.h"

#include <shlobj.h>
#include <knownfolders.h>
#include <algorithm>
#include <cctype>

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

} // namespace

bool RankBadge::ensure_wic() {
    if (wic_) return true;
    // WIC needs COM initialized on this thread.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_)));
}

ComPtr<ID2D1Bitmap>
RankBadge::load_png(ID2D1DeviceContext* d2d, const std::string& family) {
    if (dir_.empty()) return {};
    if (!ensure_wic()) return {};
    std::wstring path = dir_ + L"\\" + to_wide(family) + L".png";

    ComPtr<IWICBitmapDecoder> dec;
    if (FAILED(wic_->CreateDecoderFromFilename(path.c_str(), nullptr,
        GENERIC_READ, WICDecodeMetadataCacheOnLoad, dec.GetAddressOf())))
        return {};
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
        // Procedural shield fallback.
        auto col = tier_color(fam);
        ComPtr<ID2D1SolidColorBrush> fill, stroke, ink;
        d2d->CreateSolidColorBrush(D2D1::ColorF(col.r, col.g, col.b, 0.30f),
                                   fill.GetAddressOf());
        d2d->CreateSolidColorBrush(col, stroke.GetAddressOf());
        d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.95f),
                                   ink.GetAddressOf());

        float cx = (dst.left + dst.right) * 0.5f;
        float top = dst.top;
        float bot = dst.bottom;
        float w = (dst.right - dst.left) * 0.5f;
        float mid = top + (bot - top) * 0.55f;

        ComPtr<ID2D1Factory> f;
        d2d->GetFactory(f.GetAddressOf());
        ComPtr<ID2D1PathGeometry> path;
        f->CreatePathGeometry(path.GetAddressOf());
        ComPtr<ID2D1GeometrySink> sink;
        path->Open(sink.GetAddressOf());
        sink->BeginFigure(D2D1::Point2F(cx - w, top),
                          D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(cx + w, top));
        sink->AddLine(D2D1::Point2F(cx + w, mid));
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx + w * 0.9f, bot - 4),
            D2D1::Point2F(cx + w * 0.3f, bot),
            D2D1::Point2F(cx, bot)));
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx - w * 0.3f, bot),
            D2D1::Point2F(cx - w * 0.9f, bot - 4),
            D2D1::Point2F(cx - w, mid)));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->Close();

        d2d->FillGeometry(path.Get(), fill.Get());
        d2d->DrawGeometry(path.Get(), stroke.Get(), 1.5f);

        // Roman numeral in the shield.
        std::wstring rn = roman_from_tier(tier);
        if (!rn.empty()) {
            ComPtr<IDWriteTextFormat> fmt;
            dw->CreateTextFormat(L"Segoe UI", nullptr,
                DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                (bot - top) * 0.35f, L"en-us", fmt.GetAddressOf());
            fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            D2D1_RECT_F tr = D2D1::RectF(dst.left, top + 2,
                                          dst.right, mid);
            d2d->DrawTextW(rn.c_str(), (UINT32)rn.size(),
                           fmt.Get(), tr, ink.Get());
        }
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
