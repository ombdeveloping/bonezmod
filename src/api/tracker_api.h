#pragma once
#include <string>
#include <vector>
#include <optional>

namespace bonez {

struct RankInfo {
    std::string playlist;   // "duel", "doubles", "standard", …
    std::string tier;       // "Diamond II", …
    int         mmr = 0;
    int         wins = 0;
    int         games = 0;
};

struct TrackerProfile {
    std::string platform;   // "steam", "epic", "psn", …
    std::string handle;
    std::vector<RankInfo> ranks;
    std::string raw_json;   // for the UI to show
};

// Fetch a profile from a public tracker endpoint. Uses WinHTTP; blocking.
// Returns nullopt on any transport or parse error.
std::optional<TrackerProfile> fetch_profile(const std::string& platform,
                                            const std::string& handle);

} // namespace bonez
