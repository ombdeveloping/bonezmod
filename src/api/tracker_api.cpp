#include "api/tracker_api.h"

#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace bonez {

namespace {

std::wstring to_w(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

bool https_get(const std::wstring& host, const std::wstring& path,
               std::string& out) {
    HINTERNET s = WinHttpOpen(L"bonezmod/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!s) return false;
    HINTERNET c = WinHttpConnect(s, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!c) { WinHttpCloseHandle(s); return false; }
    HINTERNET r = WinHttpOpenRequest(c, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }

    bool ok = WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
           && WinHttpReceiveResponse(r, nullptr);
    if (ok) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(r, &avail) && avail) {
            std::vector<char> buf(avail);
            DWORD read = 0;
            if (!WinHttpReadData(r, buf.data(), avail, &read)) break;
            out.append(buf.data(), read);
        }
    }
    WinHttpCloseHandle(r);
    WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok && !out.empty();
}

// Minimal targeted scraper for numeric/string values by key. This avoids
// a JSON-lib dep; the raw body is preserved on TrackerProfile for full UI.
std::string pick_string(const std::string& body, const std::string& key) {
    std::string tag = "\"" + key + "\"";
    auto p = body.find(tag);
    if (p == std::string::npos) return {};
    p = body.find(':', p);
    if (p == std::string::npos) return {};
    ++p;
    while (p < body.size() && (body[p] == ' ' || body[p] == '"')) ++p;
    auto e = body.find_first_of("\",}", p);
    if (e == std::string::npos) return {};
    return body.substr(p, e - p);
}

} // namespace

std::optional<TrackerProfile> fetch_profile(const std::string& platform,
                                            const std::string& handle) {
    if (platform.empty() || handle.empty()) return std::nullopt;
    std::wstring host = L"api.tracker.gg";
    std::wstring path = L"/api/v2/rocket-league/standard/profile/"
                      + to_w(platform) + L"/" + to_w(handle);
    std::string body;
    if (!https_get(host, path, body)) return std::nullopt;

    TrackerProfile p;
    p.platform = platform;
    p.handle   = handle;
    p.raw_json = body;

    // Pull a small set of common fields; the UI can render raw_json for rest.
    for (const char* pl : {"duel","doubles","standard","hoops","rumble","dropshot","snowday"}) {
        std::string tag = std::string("\"") + pl + "\"";
        auto pos = body.find(tag);
        if (pos == std::string::npos) continue;
        RankInfo r;
        r.playlist = pl;
        r.tier = pick_string(body.substr(pos), "tier");
        try { r.mmr = std::stoi(pick_string(body.substr(pos), "rating")); } catch (...) {}
        p.ranks.push_back(std::move(r));
    }
    return p;
}

} // namespace bonez
