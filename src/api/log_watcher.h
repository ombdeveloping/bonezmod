#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>

namespace bonez {

// Player identity extracted from the RL client log.
struct LogPlayer {
    std::string handle;      // display name
    std::string platform;    // "steam", "epic", "psn", "xbl", "switch"
    std::string platform_id; // SteamID64 / EpicAccountId / etc. (may be empty)
    int         team = -1;   // 0 = blue, 1 = orange, -1 = unknown
};

struct MatchSnapshot {
    std::string playlist;        // "ranked-duels", "ranked-doubles", …
    std::string map;
    std::vector<LogPlayer> players;
    std::chrono::system_clock::time_point observed_at{};
};

// Tails Documents\My Games\Rocket League\TAGame\Logs\Launch.log and emits
// a MatchSnapshot whenever the roster of the current lobby changes. Runs
// on its own thread. Pure file IO — no game process contact.
class LogWatcher {
public:
    using Callback = std::function<void(const MatchSnapshot&)>;

    // Auto-locates the log via SHGetKnownFolderPath if `explicit_path`
    // is empty. Returns false if the file can't be found or opened.
    bool start(Callback cb, const std::wstring& explicit_path = L"");
    void stop();
    // Latest snapshot the tail has assembled, thread-safe copy.
    MatchSnapshot latest() const;

private:
    void run();
    void ingest_line(const std::string& line);

    std::wstring        path_;
    std::thread         th_;
    std::atomic<bool>   stop_{false};
    Callback            cb_;
    mutable std::mutex  m_;
    MatchSnapshot       cur_;
    bool                match_dirty_ = false;
};

} // namespace bonez
