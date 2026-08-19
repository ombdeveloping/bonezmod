#include "replay/replay_parser.h"

#include <windows.h>
#include <fstream>
#include <cstring>
#include <sstream>

namespace bonez {

namespace {

class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    bool ok() const { return !bad_; }
    size_t pos() const { return i_; }
    bool has(size_t k) const { return !bad_ && (i_ + k <= n_); }

    uint32_t u32() {
        if (!has(4)) { bad_ = true; return 0; }
        uint32_t v;
        std::memcpy(&v, p_ + i_, 4);
        i_ += 4;
        return v;
    }
    uint64_t u64() {
        if (!has(8)) { bad_ = true; return 0; }
        uint64_t v;
        std::memcpy(&v, p_ + i_, 8);
        i_ += 8;
        return v;
    }
    float f32() {
        uint32_t u = u32();
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    // UE3-style FString: int32 length. If positive, ASCII (null-terminated,
    // length includes null). If negative, UTF-16LE, abs(length) code units
    // (null-terminated, includes null).
    std::string str() {
        int32_t len = (int32_t)u32();
        if (bad_) return {};
        if (len == 0) return {};
        if (len > 0) {
            if (!has((size_t)len)) { bad_ = true; return {}; }
            std::string s((const char*)(p_ + i_), (size_t)len);
            i_ += (size_t)len;
            if (!s.empty() && s.back() == '\0') s.pop_back();
            return s;
        } else {
            size_t units = (size_t)(-len);
            if (!has(units * 2)) { bad_ = true; return {}; }
            std::wstring w((const wchar_t*)(p_ + i_), units);
            i_ += units * 2;
            if (!w.empty() && w.back() == L'\0') w.pop_back();
            // UTF-16 → UTF-8
            if (w.empty()) return {};
            int need = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                           nullptr, 0, nullptr, nullptr);
            std::string out(need, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                out.data(), need, nullptr, nullptr);
            return out;
        }
    }

private:
    const uint8_t* p_;
    size_t n_;
    size_t i_ = 0;
    bool bad_ = false;
};

// Parses one UProperty from the header. Returns false at "None" (end of bag).
bool read_property(Reader& r, ReplayMeta& m) {
    std::string name = r.str();
    if (!r.ok()) return false;
    if (name == "None" || name.empty()) return false;
    std::string type = r.str();
    (void)r.u64(); // value size + array index (unused for parsing meta)
    if (!r.ok()) return false;

    std::string value;
    if (type == "IntProperty")       { uint32_t v = r.u32(); value = std::to_string((int32_t)v); }
    else if (type == "FloatProperty"){ float f = r.f32();    std::ostringstream o; o << f; value = o.str(); }
    else if (type == "StrProperty" || type == "NameProperty") { value = r.str(); }
    else if (type == "ByteProperty") { std::string e = r.str(); std::string v = r.str(); value = e + "::" + v; }
    else if (type == "BoolProperty") { uint8_t b = 0; if (r.has(1)) { b = *(uint8_t*)((const uint8_t*)nullptr); } // fallback
                                       value = (b ? "true" : "false"); }
    else if (type == "QWordProperty"){ uint64_t v = r.u64(); value = std::to_string(v); }
    else if (type == "ArrayProperty"){ uint32_t n = r.u32(); value = "[array n=" + std::to_string(n) + "]"; }
    else                             { value = "<" + type + ">"; }

    if (name == "MapName")       m.map_name    = value;
    else if (name == "MatchType")m.match_type  = value;
    else if (name == "Playlist") m.playlist    = value;
    else if (name == "Team0Score") { try { m.team0_score = std::stoi(value); } catch (...) {} }
    else if (name == "Team1Score") { try { m.team1_score = std::stoi(value); } catch (...) {} }
    else if (name == "PlayerName") m.player_names.push_back(value);

    m.properties.emplace_back(std::move(name), std::move(value));
    return r.ok();
}

} // namespace

std::optional<ReplayMeta> parse_replay(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    if (buf.size() < 64) return std::nullopt;

    Reader r(buf.data(), buf.size());
    (void)r.u32(); // part1 size
    ReplayMeta m;
    m.crc1 = r.u32();
    m.engine_version    = r.u32();
    m.licensee_version  = r.u32();
    // Net version present in modern replays (engine >= 868 or so).
    if (m.engine_version >= 868) m.net_version = r.u32();
    m.game_type = r.str();

    while (r.ok()) {
        if (!read_property(r, m)) break;
    }
    if (!r.ok()) return std::nullopt;
    return m;
}

} // namespace bonez
