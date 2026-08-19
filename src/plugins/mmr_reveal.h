#pragma once
#include "api/log_watcher.h"
#include "api/tracker_api.h"
#include "overlay/overlay.h"
#include "overlay/rank_badge.h"

#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>

namespace bonez::plugins {

struct RevealEntry {
    std::string handle;
    std::string platform;
    int team = -1;

    // Filled by the tracker fetch:
    std::string current_tier;    // e.g. "Diamond II Div 3"
    int         current_mmr = 0;
    int         current_division = 0;  // 1..4, 0 = unknown
    int         peak_mmr = 0;
    int         wins = 0;
    int         games = 0;
    bool        loaded = false;
    bool        failed = false;

    // Cheap smurf heuristic: high MMR + very low games in the playlist.
    bool smurf_flag() const {
        return loaded && games < 40 && current_mmr > 1000;
    }
};

// Watches the log for lobby membership and asynchronously enriches each
// player with tracker.gg profile data for the currently-active playlist.
// Draws a per-player card row on the overlay.
class MmrReveal {
public:
    MmrReveal();
    ~MmrReveal();

    // Attach to a running LogWatcher.
    void attach(LogWatcher& lw);
    // Optional: point the badge loader at a folder of PNG icons.
    void set_badge_dir(const std::wstring& dir) { badges_.configure(dir); }
    void add_badge_dir(const std::wstring& dir) { badges_.add_dir(dir); }
    // Draw the current reveal card on the overlay.
    void draw(Overlay::PaintCtx& c);

private:
    void on_snapshot(const MatchSnapshot& s);
    void worker();

    std::mutex                m_;
    std::string               playlist_;
    std::vector<RevealEntry>  entries_;

    // fetch queue
    std::mutex                qm_;
    std::condition_variable   cv_;
    std::queue<std::pair<std::string,std::string>> pending_; // (platform, handle)
    std::atomic<bool>         stop_{false};
    std::thread               th_;

    // cache to avoid re-fetching the same handle within a session
    std::unordered_map<std::string, RevealEntry> cache_;
    mutable RankBadge badges_;
};

} // namespace bonez::plugins
