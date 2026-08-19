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

void MmrReveal::draw(Overlay::PaintCtx& c) const {
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
    const float row_h = 46.0f;
    const float w     = 460.0f;
    const float h     = 24.0f + row_h * snap.size();

    D2D1_ROUNDED_RECT box{ D2D1::RectF(pad_x, top, pad_x + w, top + h), 10, 10 };
    c.d2d->FillRoundedRectangle(box, panel.Get());

    std::wstring hdr = L"lobby [" + std::wstring(pl.begin(), pl.end()) + L"]";
    D2D1_RECT_F hr = D2D1::RectF(pad_x + 12, top + 4, pad_x + w, top + 24);
    c.d2d->DrawTextW(hdr.c_str(), (UINT32)hdr.size(), big.Get(), hr, white.Get());

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> tier_brush;
    c.d2d->CreateSolidColorBrush(D2D1::ColorF(1,1,1,1), tier_brush.GetAddressOf());

    for (size_t i = 0; i < snap.size(); ++i) {
        const auto& e = snap[i];
        float y = top + 24 + i * row_h;
        auto team_brush = (e.team == 1 ? orange.Get()
                        : e.team == 0 ? blue.Get()
                                       : dim.Get());
        // team dot
        D2D1_ELLIPSE dot{ D2D1::Point2F(pad_x + 20, y + 22), 6, 6 };
        c.d2d->FillEllipse(dot, team_brush);

        std::wstring name(e.handle.begin(), e.handle.end());

        // --- Name (left) ------------------------------------------------
        D2D1_RECT_F nr = D2D1::RectF(pad_x + 36, y + 4, pad_x + 260, y + 24);
        c.d2d->DrawTextW(name.c_str(), (UINT32)name.size(),
                         big.Get(), nr, white.Get());

        // --- Rank badge (right of name, same line) ----------------------
        if (e.loaded && !e.current_tier.empty()) {
            std::wstring badge = short_tier(e.current_tier);
            auto col = tier_color(e.current_tier);
            tier_brush->SetColor(D2D1::ColorF(col.r, col.g, col.b, 0.22f));

            float bx = pad_x + 260;
            float by = y + 4;
            float bw = 78;
            float bh = 20;
            D2D1_ROUNDED_RECT pill{ D2D1::RectF(bx, by, bx + bw, by + bh), 6, 6 };
            c.d2d->FillRoundedRectangle(pill, tier_brush.Get());
            tier_brush->SetColor(col);
            D2D1_RECT_F br = D2D1::RectF(bx, by, bx + bw, by + bh);
            // centered badge text
            Microsoft::WRL::ComPtr<IDWriteTextFormat> pillfmt;
            c.dw->CreateTextFormat(L"Segoe UI", nullptr,
                DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 13, L"en-us", pillfmt.GetAddressOf());
            pillfmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pillfmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            c.d2d->DrawTextW(badge.c_str(), (UINT32)badge.size(),
                             pillfmt.Get(), br, tier_brush.Get());

            // MMR to the right of the badge
            std::wstringstream mm;
            mm << L"mmr " << e.current_mmr;
            D2D1_RECT_F mr = D2D1::RectF(bx + bw + 8, y + 4,
                                          pad_x + w - 8, y + 24);
            c.d2d->DrawTextW(mm.str().c_str(), (UINT32)mm.str().size(),
                             big.Get(), mr, white.Get());
        } else if (!e.loaded && !e.failed) {
            D2D1_RECT_F mr = D2D1::RectF(pad_x + 260, y + 4,
                                          pad_x + w - 8, y + 24);
            c.d2d->DrawTextW(L"fetching…", 9, big.Get(), mr, dim.Get());
        } else if (e.failed) {
            D2D1_RECT_F mr = D2D1::RectF(pad_x + 260, y + 4,
                                          pad_x + w - 8, y + 24);
            c.d2d->DrawTextW(L"no profile", 10, big.Get(), mr, dim.Get());
        }

        // --- Second line: platform + peak + W/G -------------------------
        std::wstring plat(e.platform.begin(), e.platform.end());
        std::wstringstream l2;
        l2 << L"[" << plat << L"]";
        if (e.loaded) {
            l2 << L"   peak " << e.peak_mmr
               << L"   " << e.wins << L"w / " << e.games << L"g";
        }
        D2D1_RECT_F sr = D2D1::RectF(pad_x + 36, y + 24, pad_x + w - 8, y + 42);
        c.d2d->DrawTextW(l2.str().c_str(), (UINT32)l2.str().size(),
                         small.Get(), sr,
                         e.smurf_flag() ? red.Get() : dim.Get());
    }
}

} // namespace bonez::plugins
