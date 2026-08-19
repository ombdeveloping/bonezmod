#pragma once
#include <string>

namespace bonez::plugins {

// Talks to OBS Studio's obs-websocket v5 plugin over WinHTTP WebSockets.
// This is entirely out-of-process — nothing about RL is touched. Enable
// the replay buffer in OBS, set a password (or leave blank), and this
// triggers a save on hotkey.
//
// Auth flow (v5):
//   1. WebSocket handshake to ws://host:port
//   2. Server sends Hello (op=0) with challenge+salt if auth enabled
//   3. Client sends Identify (op=1) with SHA-256(base64(SHA-256(pw+salt))+challenge)
//   4. Server sends Identified (op=2)
//   5. Client sends Request (op=6) with requestType="SaveReplayBuffer"
class ObsReplay {
public:
    void configure(const std::wstring& host, int port,
                   const std::string& password) {
        host_ = host; port_ = port; pw_ = password;
    }
    bool save_replay_buffer();
    bool start_replay_buffer();
    bool stop_replay_buffer();

private:
    bool send_request(const std::string& request_type);

    std::wstring host_ = L"127.0.0.1";
    int port_ = 4455;
    std::string pw_;
};

} // namespace bonez::plugins
