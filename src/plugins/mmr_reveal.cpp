#include "plugins/mmr_reveal.h"

#include <wrl/client.h>
#include <d2d1_1.h>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace bonez::plugins {

namespace {

// Map a log playlist name to a tracker.gg playlist key.
std::string tracker_key(const std::string& log_playlist) {
    if (log_playlist == "ranked-duels")     return "duel";
    if (log_playlist == "ranked-doubles")   return "doubles";
    if (log_playlist == "ranked-standard")  return "standard";
    if (log_playlist == "hoops")            return "hoops";
    if (log_playlist == "rumble")           return "rumble";
    if (log_playlist == "dropshot")         return "dropshot";
    if (log_playlist == "snowday")          return "snowday";
    return "standard";
}

// Very small numeric extractor: finds the first N in "key":N inside a JSON
// blob without dragging in a JSON dep.
int pick_int(const std::string& j, const std::string& key) {
    std::string t = "\"" + key + "\"";
    auto p = j.find(t);
    if (p == std::string::npos) return 0;
    p = j.find(':', p);
    if (p == std::string::npos) return 0;
    ++p;
    while (p < j.size() && (j[p] == ' ' || j[p] == '"')) ++p;
    int sign = 1;
    if (p < j.size() && j[p] == '-') { sign = -1; ++p; }
    int n = 0;
    while (p < j.size() && j[p] >= '0' && j[p] <= '9') {
        n = n * 10 + (j[p] - '0');
        ++p;
    }
    return sign * n;
}

// Rank-tier -> accent color. The name in tracker.gg tiers reads
// "Bronze I", "Silver II Div 3", "Grand Champion I", "Supersonic Legend".
D2D1::ColorF tier_color(const std::string& tier) {
    std::string t = tier;
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    if (t.find("supersonic")     != std::string::npos) return D2D1::ColorF(1.00f, 0.35f, 0.90f); // pink
    if (t.find("grand champion") != std::string::npos) return D2D1::ColorF(0.75f, 0.45f, 1.00f); // purple
    if (t.find("champion")       != std::string::npos) return D2D1::ColorF(0.55f, 0.70f, 1.00f); // blue-violet
    if (t.find("diamond")        != std::string::npos) return D2D1::ColorF(0.35f, 0.85f, 1.00f); // cyan
    if (t.find("platinum")       != std::string::npos) return D2D1::ColorF(0.55f, 1.00f, 0.85f); // teal
    if (t.find("gold")           != std::string::npos) return D2D1::ColorF(1.00f, 0.85f, 0.30f); // gold
    if (t.find("silver")         != std::string::npos) return D2D1::ColorF(0.85f, 0.85f, 0.90f); // silver
    if (t.find("bronze")         != std::string::npos) return D2D1::ColorF(0.85f, 0.55f, 0.35f); // bronze
    return D2D1::ColorF(1, 1, 1, 0.75f);
}

// Compress "Grand Champion II Div 3" -> "GC2 d3" for the inline badge.
std::wstring short_tier(const std::string& tier) {
    std::string t = tier;
    auto lower = t;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    std::string prefix;
    if      (lower.find("supersonic")     != std::string::npos) prefix = "SSL";
    else if (lower.find("grand champion") != std::string::npos) prefix = "GC";
    else if (lower.find("champion")       != std::string::npos) prefix = "C";
    else if (lower.find("diamond")        != std::string::npos) prefix = "D";
    else if (lower.find("platinum")       != std::string::npos) prefix = "P";
    else if (lower.find("gold")           != std::string::npos) prefix = "G";
    else if (lower.find("silver")         != std::string::npos) prefix = "S";
    else if (lower.find("bronze")         != std::string::npos) prefix = "B";
    else                                                        prefix = tier;

    // Trailing roman numeral.
    std::string tail;
    auto pos = lower.find_last_of(" ");
    while (pos != std::string::npos) {
        std::string tok = lower.substr(pos + 1);
        if (tok == "i")   { tail = "1"; break; }
        if (tok == "ii")  { tail = "2"; break; }
        if (tok == "iii") { tail = "3"; break; }
        if (tok == "iv")  { tail = "4"; break; }
        if (tok == "v")   { tail = "5"; break; }
        if (pos == 0) break;
        pos = lower.find_last_of(" ", pos - 1);
    }

    // Division suffix if present.
    std::string div;
    auto dp = lower.find(" div ");
    if (dp != std::string::npos && dp + 5 < lower.size())
        div = std::string(" d") + lower[dp + 5];

    std::string out = prefix + tail + div;
    return std::wstring(out.begin(), out.end());
}

std::string pick_str(const std::string& j, const std::string& key) {
    std::string t = "\"" + key + "\"";
    auto p = j.find(t);
    if (p == std::string::npos) return {};
    p = j.find('"', j.find(':', p) + 1);
    if (p == std::string::npos) return {};
    ++p;
    auto e = j.find('"', p);
    if (e == std::string::npos) return {};
    return j.substr(p, e - p);
}

} // namespace

MmrReveal::MmrReveal() {
    th_ = std::thread([this]{ worker(); });
}
MmrReveal::~MmrReveal() {
    stop_ = true;
    cv_.notify_all();
    if (th_.joinable()) th_.join();
}

void MmrReveal::attach(LogWatcher& lw) {
    lw.start([this](const MatchSnapshot& s){ on_snapshot(s); });
}

void MmrReveal::on_snapshot(const MatchSnapshot& s) {
    {
        std::lock_guard<std::mutex> lk(m_);
        playlist_ = s.playlist;
        // Rebuild entry list from the new snapshot, preserving cached data.
        std::vector<RevealEntry> next;
        for (auto& p : s.players) {
            RevealEntry e;
            e.handle = p.handle;
            e.platform = p.platform;
            e.team = p.team;
            auto key = p.platform + "/" + p.handle;
            auto it = cache_.find(key);
            if (it != cache_.end()) e = it->second;
            e.handle = p.handle;
            e.platform = p.platform;
            e.team = p.team;
            next.push_back(std::move(e));
        }
        entries_ = std::move(next);
    }
    // Queue unfetched players.
    for (auto& p : s.players) {
        auto key = p.platform + "/" + p.handle;
        if (cache_.find(key) != cache_.end()) continue;
        std::lock_guard<std::mutex> lk(qm_);
        pending_.emplace(p.platform, p.handle);
        cv_.notify_one();
    }
}

void MmrReveal::worker() {
    while (!stop_) {
        std::pair<std::string,std::string> job;
        {
            std::unique_lock<std::mutex> lk(qm_);
            cv_.wait(lk, [&]{ return stop_ || !pending_.empty(); });
            if (stop_) return;
            job = pending_.front();
            pending_.pop();
        }
        auto prof = fetch_profile(job.first, job.second);
        RevealEntry e;
        e.platform = job.first;
        e.handle   = job.second;
        e.loaded   = prof.has_value();
        e.failed   = !prof.has_value();

        if (prof) {
            std::string tk;
            {
                std::lock_guard<std::mutex> lk(m_);
                tk = tracker_key(playlist_);
            }
            std::string j = prof->raw_json;
            // Narrow to the playlist section, then pull common fields.
            auto pos = j.find("\"" + tk + "\"");
            if (pos != std::string::npos) {
                std::string sub = j.substr(pos, 2048);
                e.current_mmr  = pick_int(sub, "rating");
                e.peak_mmr     = pick_int(sub, "peakRating");
                e.wins         = pick_int(sub, "wins");
                e.games        = pick_int(sub, "matchesPlayed");
                if (!e.games)  e.games = pick_int(sub, "gamesPlayed");
                e.current_tier = pick_str(sub, "tier");
                if (e.current_tier.empty())
                    e.current_tier = pick_str(sub, "divisionName");
                int div = pick_int(sub, "division");
                if (div < 1 || div > 4) {
                    // tracker sometimes reports division inside the tier
                    // string as "Div N"; scavenge that.
                    auto dp = e.current_tier.find("Div ");
                    if (dp != std::string::npos && dp + 4 < e.current_tier.size())
                        div = e.current_tier[dp + 4] - '0';
                }
                e.current_division = (div >= 1 && div <= 4) ? div : 0;
            }
        }

        {
            std::lock_guard<std::mutex> lk(m_);
            cache_[e.platform + "/" + e.handle] = e;
            for (auto& x : entries_)
                if (x.platform == e.platform && x.handle == e.handle) {
                    int t = x.team;
                    x = e;
                    x.team = t;
                }
        }
    }
}

void MmrReveal::draw(Overlay::PaintCtx& c) {
    std::vector<RevealEntry> snap;
    std::string pl;
    {
        std::lock_guard<std::mutex> lk(m_);
        snap = entries_;
        pl   = playlist_;
    }
    if (snap.empty()) return;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panel, blue, orange,
        white, dim, red;
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(0,0,0,0.55f), panel.GetAddressOf());
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(0.30f,0.55f,1.0f,1), blue.GetAddressOf());
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(1.0f,0.55f,0.15f,1), orange.GetAddressOf());
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(1,1,1,1), white.GetAddressOf());
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(1,1,1,0.55f), dim.GetAddressOf());
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(1.0f,0.25f,0.25f,1), red.GetAddressOf());

    Microsoft::WRL::ComPtr<IDWriteTextFormat> big, small;
    c.dw->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 16, L"en-us", big.GetAddressOf());
    c.dw->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 12, L"en-us", small.GetAddressOf());

    const float pad_x = 16.0f;
    const float top   = 148.0f;
    const float row_h = 52.0f;
    const float w     = 480.0f;
    const float h     = 24.0f + row_h * snap.size();

    D2D1_ROUNDED_RECT box{ D2D1::RectF(pad_x, top, pad_x + w, top + h), 10, 10 };
    c.d2d->FillRoundedRectangle(box, panel.Get());

    std::wstring hdr = L"lobby [" + std::wstring(pl.begin(), pl.end()) + L"]";
    D2D1_RECT_F hr = D2D1::RectF(pad_x + 12, top + 4, pad_x + w, top + 24);
    c.d2d->DrawTextW(hdr.c_str(), (UINT32)hdr.size(), big.Get(), hr, white.Get());

    Microsoft::WRL::ComPtr<IDWriteTextFormat> bang;
    c.dw->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 22, L"en-us", bang.GetAddressOf());
    bang->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    bang->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    for (size_t i = 0; i < snap.size(); ++i) {
        const auto& e = snap[i];
        float y = top + 24 + i * row_h;
        auto team_brush = (e.team == 1 ? orange.Get()
                        : e.team == 0 ? blue.Get()
                                       : dim.Get());

        // team dot
        D2D1_ELLIPSE dot{ D2D1::Point2F(pad_x + 12, y + 24), 5, 5 };
        c.d2d->FillEllipse(dot, team_brush);

        // --- Rank badge (leftmost after team dot) -----------------------
        float badge_x = pad_x + 24;
        float badge_size = 40.0f;
        D2D1_RECT_F bdst = D2D1::RectF(badge_x, y + 2,
                                        badge_x + badge_size,
                                        y + 2 + badge_size);
        if (e.loaded && !e.current_tier.empty()) {
            badges_.draw(c.d2d, c.dw, e.current_tier,
                         e.current_division, bdst);
        }

        // --- Smurf "!" glyph, drawn over the badge's top-right corner ---
        if (e.smurf_flag()) {
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> ring;
            c.d2d->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0, 0, 0.9f),
                                         ring.GetAddressOf());
            D2D1_ELLIPSE badge_dot{
                D2D1::Point2F(bdst.right - 2, bdst.top + 2), 10, 10 };
            c.d2d->FillEllipse(badge_dot, red.Get());
            c.d2d->DrawEllipse(badge_dot, ring.Get(), 1.5f);
            D2D1_RECT_F br = D2D1::RectF(bdst.right - 12, bdst.top - 8,
                                          bdst.right + 8, bdst.top + 12);
            c.d2d->DrawTextW(L"!", 1, bang.Get(), br, white.Get());
        }

        // --- Name (right of badge) --------------------------------------
        float text_x = badge_x + badge_size + 12;
        std::wstring name(e.handle.begin(), e.handle.end());
        D2D1_RECT_F nr = D2D1::RectF(text_x, y + 4,
                                      pad_x + w - 100, y + 24);
        c.d2d->DrawTextW(name.c_str(), (UINT32)name.size(),
                         big.Get(), nr, white.Get());

        // --- MMR (right side, big) --------------------------------------
        Microsoft::WRL::ComPtr<IDWriteTextFormat> mmrfmt;
        c.dw->CreateTextFormat(L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 17, L"en-us", mmrfmt.GetAddressOf());
        mmrfmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        std::wstring mmr_line;
        if (e.loaded)      mmr_line = std::to_wstring(e.current_mmr);
        else if (e.failed) mmr_line = L"—";
        else               mmr_line = L"…";
        auto col = e.loaded ? tier_color(e.current_tier)
                            : D2D1::ColorF(1, 1, 1, 0.55f);
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> mmrbr;
        c.d2d->CreateSolidColorBrush(col, mmrbr.GetAddressOf());
        D2D1_RECT_F mr = D2D1::RectF(pad_x + w - 100, y + 4,
                                      pad_x + w - 12, y + 26);
        c.d2d->DrawTextW(mmr_line.c_str(), (UINT32)mmr_line.size(),
                         mmrfmt.Get(), mr, mmrbr.Get());

        // --- Second line: tier text + platform + peak + W/G -------------
        std::wstring plat(e.platform.begin(), e.platform.end());
        std::wstringstream l2;
        if (e.loaded && !e.current_tier.empty()) {
            l2 << std::wstring(e.current_tier.begin(), e.current_tier.end())
               << L"   ";
        }
        l2 << L"[" << plat << L"]";
        if (e.loaded) {
            l2 << L"   peak " << e.peak_mmr
               << L"   " << e.wins << L"w / " << e.games << L"g";
        }
        D2D1_RECT_F sr = D2D1::RectF(text_x, y + 26,
                                      pad_x + w - 12, y + 46);
        c.d2d->DrawTextW(l2.str().c_str(), (UINT32)l2.str().size(),
                         small.Get(), sr,
                         e.smurf_flag() ? red.Get() : dim.Get());
    }
}

} // namespace bonez::plugins
