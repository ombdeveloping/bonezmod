#include "plugins/obs_replay.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace bonez::plugins {

namespace {

std::string b64(const std::vector<uint8_t>& in) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, bits = -6;
    for (uint8_t c : in) {
        val = (val << 8) + c; bits += 8;
        while (bits >= 0) {
            out.push_back(T[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(T[((val << 8) >> (bits + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::vector<uint8_t> sha256(const std::string& data) {
    BCRYPT_ALG_HANDLE h;
    BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    DWORD hlen = 0, cb = 0;
    BCryptGetProperty(h, BCRYPT_HASH_LENGTH, (PUCHAR)&hlen, sizeof(hlen), &cb, 0);
    std::vector<uint8_t> out(hlen);
    BCRYPT_HASH_HANDLE hh;
    BCryptCreateHash(h, &hh, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hh, (PUCHAR)data.data(), (ULONG)data.size(), 0);
    BCryptFinishHash(hh, out.data(), hlen, 0);
    BCryptDestroyHash(hh);
    BCryptCloseAlgorithmProvider(h, 0);
    return out;
}

std::string extract_string(const std::string& j, const std::string& key) {
    std::string tag = "\"" + key + "\"";
    auto p = j.find(tag);
    if (p == std::string::npos) return {};
    p = j.find('\"', j.find(':', p) + 1);
    if (p == std::string::npos) return {};
    ++p;
    auto e = j.find('\"', p);
    if (e == std::string::npos) return {};
    return j.substr(p, e - p);
}

// Minimal RFC6455 unmasked-server / masked-client framing helpers.
std::vector<uint8_t> make_text_frame(const std::string& payload) {
    std::vector<uint8_t> f;
    f.push_back(0x81); // FIN | text
    uint32_t mask = 0xA5A5A5A5;
    size_t n = payload.size();
    if (n < 126) f.push_back((uint8_t)(0x80 | n));
    else if (n <= 0xFFFF) {
        f.push_back(0x80 | 126);
        f.push_back((n >> 8) & 0xFF);
        f.push_back(n & 0xFF);
    } else {
        f.push_back(0x80 | 127);
        for (int i = 7; i >= 0; --i) f.push_back((uint8_t)((uint64_t)n >> (i*8)));
    }
    for (int i = 3; i >= 0; --i) f.push_back((mask >> (i*8)) & 0xFF);
    for (size_t i = 0; i < n; ++i)
        f.push_back((uint8_t)payload[i] ^ ((mask >> ((3 - (i % 4)) * 8)) & 0xFF));
    return f;
}

// Read one frame (assumes server payload < 64KiB — obs-websocket messages
// stay small for control ops).
bool read_frame(HINTERNET ws, std::string& out) {
    uint8_t hdr[2];
    DWORD read = 0;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE bt;
    // WinHTTP handles framing for us via WinHttpWebSocketReceive.
    std::vector<uint8_t> buf(8192);
    DWORD total = 0;
    for (;;) {
        DWORD got = 0;
        DWORD rc = WinHttpWebSocketReceive(ws, buf.data() + total,
                                           (DWORD)(buf.size() - total), &got, &bt);
        if (rc != NO_ERROR) return false;
        total += got;
        if (bt == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
            bt == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) break;
        if (bt == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) return false;
        if (total >= buf.size()) buf.resize(buf.size() * 2);
    }
    out.assign((char*)buf.data(), total);
    (void)hdr; (void)read;
    return true;
}

bool send_text(HINTERNET ws, const std::string& s) {
    return WinHttpWebSocketSend(ws,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        (PVOID)s.data(), (DWORD)s.size()) == NO_ERROR;
}

} // namespace

bool ObsReplay::send_request(const std::string& request_type) {
    HINTERNET session = WinHttpOpen(L"bonezmod-obs/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) return false;

    HINTERNET conn = WinHttpConnect(session, host_.c_str(), (INTERNET_PORT)port_, 0);
    if (!conn) { WinHttpCloseHandle(session); return false; }

    HINTERNET req = WinHttpOpenRequest(conn, L"GET", L"/",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!req) { WinHttpCloseHandle(conn); WinHttpCloseHandle(session); return false; }

    // Upgrade the request to a WebSocket handshake.
    WinHttpSetOption(req, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);
    bool ok = false;
    HINTERNET ws = nullptr;
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
        ws = WinHttpWebSocketCompleteUpgrade(req, 0);
    }
    WinHttpCloseHandle(req);
    if (!ws) { WinHttpCloseHandle(conn); WinHttpCloseHandle(session); return false; }

    // 1) Read Hello (op=0)
    std::string hello;
    if (!read_frame(ws, hello)) goto done;

    // 2) Compute auth response if required.
    {
        std::string identify;
        auto chal = extract_string(hello, "challenge");
        auto salt = extract_string(hello, "salt");
        if (!chal.empty() && !salt.empty() && !pw_.empty()) {
            auto secret = b64(sha256(pw_ + salt));
            auto auth   = b64(sha256(secret + chal));
            identify = "{\"op\":1,\"d\":{\"rpcVersion\":1,"
                       "\"authentication\":\"" + auth + "\","
                       "\"eventSubscriptions\":0}}";
        } else {
            identify = "{\"op\":1,\"d\":{\"rpcVersion\":1,"
                       "\"eventSubscriptions\":0}}";
        }
        if (!send_text(ws, identify)) goto done;
    }

    // 3) Wait for Identified.
    {
        std::string ident;
        if (!read_frame(ws, ident)) goto done;
        if (ident.find("\"op\":2") == std::string::npos) goto done;
    }

    // 4) Send Request.
    {
        std::string uuid = "bonez-req-1";
        std::string req_msg =
            "{\"op\":6,\"d\":{\"requestType\":\"" + request_type + "\","
            "\"requestId\":\"" + uuid + "\"}}";
        if (!send_text(ws, req_msg)) goto done;

        std::string resp;
        if (!read_frame(ws, resp)) goto done;
        ok = resp.find("\"requestStatus\"") != std::string::npos &&
             resp.find("\"result\":true") != std::string::npos;
    }

done:
    if (ws) {
        WinHttpWebSocketClose(ws, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                              nullptr, 0);
        WinHttpCloseHandle(ws);
    }
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return ok;
}

bool ObsReplay::save_replay_buffer()  { return send_request("SaveReplayBuffer"); }
bool ObsReplay::start_replay_buffer() { return send_request("StartReplayBuffer"); }
bool ObsReplay::stop_replay_buffer()  { return send_request("StopReplayBuffer"); }

} // namespace bonez::plugins
