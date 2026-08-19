#include "plugins/mmr_reveal.h"

#include <wrl/client.h>
#include <d2d1_1.h>
#include <sstream>

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
        std::wstring plat(e.platform.begin(), e.platform.end());
        std::wstringstream ss;
        ss << name << L"   [" << plat << L"]";
        D2D1_RECT_F nr = D2D1::RectF(pad_x + 36, y + 4, pad_x + w - 8, y + 24);
        c.d2d->DrawTextW(ss.str().c_str(), (UINT32)ss.str().size(),
                         big.Get(), nr, white.Get());

        std::wstring line2;
        if (e.failed) {
            line2 = L"tracker: no profile";
        } else if (!e.loaded) {
            line2 = L"tracker: fetching…";
        } else {
            std::wstringstream l2;
            l2 << std::wstring(e.current_tier.begin(), e.current_tier.end())
               << L"   mmr " << e.current_mmr
               << L" (peak " << e.peak_mmr << L")"
               << L"   " << e.wins << L"w / " << e.games << L"g";
            line2 = l2.str();
        }
        D2D1_RECT_F sr = D2D1::RectF(pad_x + 36, y + 22, pad_x + w - 8, y + 42);
        c.d2d->DrawTextW(line2.c_str(), (UINT32)line2.size(),
                         small.Get(), sr,
                         e.smurf_flag() ? red.Get() : dim.Get());
    }
}

} // namespace bonez::plugins
