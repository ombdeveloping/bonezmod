#pragma once
#include <windows.h>
#include <string>

namespace bonez {

struct GameProcess {
    DWORD pid = 0;
    HWND  hwnd = nullptr;
    RECT  client_rect{};    // in screen coords, top-left/bottom-right
    bool  fullscreen = false;
};

// Locate RocketLeague.exe by image name and its top-level window.
// Never calls OpenProcess with VM_READ or QUERY_INFORMATION on it.
bool find_rocket_league(GameProcess& out);

} // namespace bonez
