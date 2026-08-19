#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace bonez {

// Minimal offline parse of a Rocket League .replay file. Only reads the
// meta section (properties). This file lives entirely on disk, so parsing
// it never touches the game process and is EAC-irrelevant.
struct ReplayMeta {
    uint32_t crc1 = 0, crc2 = 0;
    uint32_t engine_version = 0, licensee_version = 0, net_version = 0;
    std::string game_type;
    std::string map_name;
    std::string playlist;
    std::string match_type;
    int         team0_score = 0;
    int         team1_score = 0;
    std::vector<std::string> player_names;
    // Property bag preserved verbatim for the UI.
    std::vector<std::pair<std::string, std::string>> properties;
};

// Returns nullopt on any structural error. Non-throwing.
std::optional<ReplayMeta> parse_replay(const std::wstring& path);

} // namespace bonez
