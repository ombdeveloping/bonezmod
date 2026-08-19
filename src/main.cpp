// bonezmod — MMR reveal, external and read-only.
//
// Positions a transparent, click-through overlay over the Rocket League
// window and paints a per-player rank + MMR card sourced from the client
// log and tracker.gg. Zero contact with RocketLeague.exe: no injection,
// no memory access, no input emission.
//
// Hotkeys (global):
//   Ctrl+Alt+T   toggle overlay visibility
//   Ctrl+Alt+F3  toggle MMR card
//   Ctrl+Alt+Q   quit

#include "eac_guard.h"
#include "process_watch.h"
#include "overlay/overlay.h"
#include "api/log_watcher.h"
#include "plugins/mmr_reveal.h"

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace bonez;

namespace {

enum HotkeyId : int {
    HK_TOGGLE_OVR = 1,
    HK_TOG_REVEAL = 2,
    HK_QUIT       = 3,
};

void register_hotkeys() {
    RegisterHotKey(nullptr, HK_TOGGLE_OVR, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T');
    RegisterHotKey(nullptr, HK_TOG_REVEAL, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F3);
    RegisterHotKey(nullptr, HK_QUIT,       MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q');
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Overlay             overlay;
    LogWatcher          log_watch;
    plugins::MmrReveal  mmr;

    // Register badge search paths: %APPDATA%\bonezmod\ranks first,
    // then <exe_dir>\assets\ranks.
    {
        PWSTR appdata = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0,
                                           nullptr, &appdata))) {
            std::wstring root = std::wstring(appdata) + L"\\bonezmod";
            std::wstring bd   = root + L"\\ranks";
            CreateDirectoryW(root.c_str(), nullptr);
            CreateDirectoryW(bd.c_str(),   nullptr);
            mmr.set_badge_dir(bd);
            CoTaskMemFree(appdata);
        }
        wchar_t exe[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (n > 0) {
            std::wstring p(exe, n);
            auto slash = p.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
                mmr.add_badge_dir(p.substr(0, slash) + L"\\assets\\ranks");
        }
    }
    mmr.attach(log_watch);

    // Initial position: primary monitor until RL is found.
    HMONITOR primary = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(primary, &mi);
    overlay.init(mi.rcMonitor);
    overlay.set_click_through(true);

    register_hotkeys();

    std::atomic<bool> reveal_on { true };
    std::atomic<bool> overlay_on{ true };

    overlay.set_paint([&](Overlay::PaintCtx& c) {
        if (reveal_on) mmr.draw(c);
    });

    auto last_scan = std::chrono::steady_clock::now();
    for (;;) {
        // Reposition + EAC status check @ ~4 Hz.
        auto now = std::chrono::steady_clock::now();
        if (now - last_scan > std::chrono::milliseconds(250)) {
            last_scan = now;
            GameProcess gp;
            if (find_rocket_league(gp)) {
                overlay.reposition(gp.client_rect);
                // EAC probe is informational only; we never emit input.
                (void)check_eac(gp.pid);
            }
        }

        // Hotkey drain.
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) {
                switch (msg.wParam) {
                    case HK_TOGGLE_OVR:
                        overlay_on = !overlay_on;
                        overlay.set_visible(overlay_on);
                        break;
                    case HK_TOG_REVEAL:
                        reveal_on = !reveal_on;
                        break;
                    case HK_QUIT:
                        goto done;
                }
            } else if (msg.message == WM_QUIT) {
                goto done;
            } else {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        if (!overlay.pump_and_render()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
done:
    UnregisterHotKey(nullptr, HK_TOGGLE_OVR);
    UnregisterHotKey(nullptr, HK_TOG_REVEAL);
    UnregisterHotKey(nullptr, HK_QUIT);

    log_watch.stop();
    overlay.shutdown();
    return 0;
}
