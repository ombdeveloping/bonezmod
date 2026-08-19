#include "api/log_watcher.h"

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <fstream>
#include <regex>
#include <chrono>
#include <algorithm>

namespace bonez {

namespace {

std::wstring default_log_path() {
    PWSTR docs = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)))
        return {};
    std::wstring p = docs;
    CoTaskMemFree(docs);
    p += L"\\My Games\\Rocket League\\TAGame\\Logs\\Launch.log";
    return p;
}

// The RL client log emits a few consistent lines during match setup:
//   PsyNet: player added   PlayerName=<name>   Platform=<id>   ...
//   OnlineGame: TeamNum=<0|1>  Player=<name>
//   LoadMap: <map> playlist=<id>
// Regex forms below are conservative and match multiple RL versions.
const std::regex kAdd(R"(Player(?:Name|Handle)="?([^"\s]+)"?.*?Platform=(\w+).*?(?:UniqueId|PlatformId)=([^\s"]+))",
                      std::regex::icase);
const std::regex kTeam(R"(Team(?:Num|Idx)=(\d).*?Player(?:Name|Handle)="?([^"\s]+)"?)",
                       std::regex::icase);
const std::regex kMap(R"(LoadMap.*?[/\\](\w+)(?:\.upk|\.udk|_P)?\b.*?[Pp]laylist=(\d+))",
                      std::regex::icase);
const std::regex kLeave(R"(Player(?:Name|Handle)="?([^"\s]+)"?.*?(?:removed|disconnected|left))",
                        std::regex::icase);

std::string playlist_name(const std::string& id) {
    // Common Psyonix playlist IDs; unknown IDs pass through as the number.
    static const std::pair<const char*, const char*> t[] = {
        {"1",  "casual-duels"},   {"2",  "casual-doubles"},
        {"3",  "casual-standard"},{"4",  "casual-chaos"},
        {"10", "ranked-duels"},   {"11", "ranked-doubles"},
        {"13", "ranked-standard"},{"27", "hoops"},
        {"28", "rumble"},         {"29", "dropshot"},
        {"30", "snowday"},        {"34", "tournaments"},
    };
    for (auto& kv : t) if (id == kv.first) return kv.second;
    return "playlist:" + id;
}

} // namespace

bool LogWatcher::start(Callback cb, const std::wstring& explicit_path) {
    path_ = explicit_path.empty() ? default_log_path() : explicit_path;
    if (path_.empty()) return false;
    cb_ = std::move(cb);
    stop_ = false;
    th_ = std::thread([this]{ run(); });
    return true;
}

void LogWatcher::stop() {
    stop_ = true;
    if (th_.joinable()) th_.join();
}

MatchSnapshot LogWatcher::latest() const {
    std::lock_guard<std::mutex> lk(m_);
    return cur_;
}

void LogWatcher::ingest_line(const std::string& line) {
    std::smatch m;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(m_);
        if (std::regex_search(line, m, kMap)) {
            std::string map = m[1].str();
            std::string pl_id = m[2].str();
            if (map != cur_.map) {
                // New map = new match: clear roster.
                cur_.map = map;
                cur_.playlist = playlist_name(pl_id);
                cur_.players.clear();
                changed = true;
            }
        } else if (std::regex_search(line, m, kAdd)) {
            LogPlayer p;
            p.handle      = m[1].str();
            p.platform    = m[2].str();
            p.platform_id = m[3].str();
            bool dup = false;
            for (auto& e : cur_.players)
                if (e.handle == p.handle) { dup = true; break; }
            if (!dup) { cur_.players.push_back(std::move(p)); changed = true; }
        } else if (std::regex_search(line, m, kTeam)) {
            int team = std::stoi(m[1].str());
            std::string h = m[2].str();
            for (auto& e : cur_.players)
                if (e.handle == h && e.team != team) { e.team = team; changed = true; break; }
        } else if (std::regex_search(line, m, kLeave)) {
            std::string h = m[1].str();
            auto before = cur_.players.size();
            cur_.players.erase(std::remove_if(cur_.players.begin(),
                cur_.players.end(),
                [&](const LogPlayer& e){ return e.handle == h; }),
                cur_.players.end());
            if (cur_.players.size() != before) changed = true;
        }
        if (changed) {
            cur_.observed_at = std::chrono::system_clock::now();
            match_dirty_ = true;
        }
    }
    if (changed && cb_) cb_(latest());
}

void LogWatcher::run() {
    // Open the file shared for read/write/delete so RL keeps writing to it.
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int tries = 0; tries < 60 && !stop_; ++tries) {
        h = CreateFileW(path_.c_str(), GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (h == INVALID_HANDLE_VALUE) return;

    // Seek to end; new sessions overwrite. Rewind on truncation.
    LARGE_INTEGER pos{}; pos.QuadPart = 0;
    SetFilePointerEx(h, pos, nullptr, FILE_END);

    std::string carry;
    char buf[4096];
    while (!stop_) {
        DWORD got = 0;
        BOOL ok = ReadFile(h, buf, sizeof(buf), &got, nullptr);
        if (ok && got > 0) {
            carry.append(buf, got);
            for (;;) {
                auto nl = carry.find('\n');
                if (nl == std::string::npos) break;
                std::string line = carry.substr(0, nl);
                carry.erase(0, nl + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                ingest_line(line);
            }
        } else {
            // EOF: detect truncation (file smaller than our position).
            LARGE_INTEGER cur{}, size{};
            SetFilePointerEx(h, {}, &cur, FILE_CURRENT);
            GetFileSizeEx(h, &size);
            if (size.QuadPart < cur.QuadPart) {
                LARGE_INTEGER z{};
                SetFilePointerEx(h, z, nullptr, FILE_BEGIN);
                carry.clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    CloseHandle(h);
}

} // namespace bonez
