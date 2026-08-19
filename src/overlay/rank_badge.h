#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace bonez {

// Draws a Rocket League rank badge for a given tier string
// (e.g. "Grand Champion II Div 3"). If a PNG named after the tier family
// exists in the user's badge directory it's blitted; otherwise a procedural
// shield in the tier's accent color is drawn. Division is shown as small
// dots along the bottom of the badge.
class RankBadge {
public:
    // One or more directories searched in order for badge PNGs:
    //   <dir>\bronze.png, silver.png, gold.png, platinum.png, diamond.png,
    //   champion.png, grand_champion.png, supersonic_legend.png
    // Missing files fall back to procedural draw.
    void configure(const std::wstring& badge_dir) { dirs_ = { badge_dir }; }
    void add_dir(const std::wstring& d) { dirs_.push_back(d); }

    // Draws inside `dst` (typically 40x40 or so, respects aspect).
    void draw(ID2D1DeviceContext* d2d,
              IDWriteFactory*     dw,
              const std::string&  tier,
              int                 division_1_to_4,   // 1..4, 0 = unknown
              const D2D1_RECT_F&  dst);

private:
    void draw_procedural(ID2D1DeviceContext* d2d,
                         IDWriteFactory*     dw,
                         const std::string&  family,
                         const std::string&  full_tier,
                         const D2D1_RECT_F&  dst);

    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_;
    std::unordered_map<std::string,
        Microsoft::WRL::ComPtr<ID2D1Bitmap>> cache_;
    // "loaded" markers so a missing file doesn't hit the disk every frame.
    std::unordered_map<std::string, bool> tried_;
    std::vector<std::wstring> dirs_;

    bool ensure_wic();
    Microsoft::WRL::ComPtr<ID2D1Bitmap>
        load_png(ID2D1DeviceContext* d2d, const std::string& family);
};

} // namespace bonez
