// bonezmod entry point.
//
// External, EAC-safe Rocket League companion. This process:
//   * never opens RocketLeague.exe with PROCESS_VM_READ/WRITE
//   * never injects a DLL
//   * uses DXGI Desktop Duplication for screen state
//   * uses ViGEmBus for input (virtual Xbox 360 pad)
//   * paints a transparent DirectComposition overlay on top
//   * hard-disarms the pad + macros when EAC is detected
//
// Hotkeys (global):
//   Ctrl+Alt+R   full freeplay reset (menu-nav macro)
//   Ctrl+Alt+E   in-training shot reset
//   Ctrl+Alt+T   toggle overlay
//   Ctrl+Alt+M   toggle click-through
//   Ctrl+Alt+Q   quit

#include "eac_guard.h"
#include "process_watch.h"
#include "capture/dxgi_capture.h"
#include "vision/detector.h"
#include "overlay/overlay.h"
#include "input/vigem_pad.h"
#include "input/macro.h"
#include "plugins/freeplay_reset.h"
#include "plugins/auto_queue.h"
#include "plugins/ball_trail.h"
#include "plugins/obs_replay.h"
#include "plugins/chat.h"
#include "plugins/camera_preset.h"
#include "plugins/mmr_reveal.h"
#include "api/log_watcher.h"
#include "api/tracker_api.h"
#include "ui/controlpanel.h"

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <chrono>
#include <thread>
#include <string>
#include <sstream>

using namespace bonez;

namespace {

enum HotkeyId : int {
    HK_RESET_FULL = 1,
    HK_RESET_SHOT = 2,
    HK_TOGGLE_OVR = 3,
    HK_TOGGLE_CT  = 4,
    HK_QUIT       = 5,
    HK_AUTO_GG    = 6,
    HK_WHAT_SAVE  = 7,
    HK_NICE_SHOT  = 8,
    HK_REQUEUE    = 9,
    HK_CAM_APPLY  = 10,
    HK_OBS_CLIP   = 11,
    HK_TOG_TRAIL  = 15,
    HK_TOG_AUTOQ  = 16,
    HK_TOG_REVEAL = 17,
};

void register_hotkeys() {
    RegisterHotKey(nullptr, HK_RESET_FULL, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'R');
    RegisterHotKey(nullptr, HK_RESET_SHOT, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'E');
    RegisterHotKey(nullptr, HK_TOGGLE_OVR, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T');
    RegisterHotKey(nullptr, HK_TOGGLE_CT,  MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'M');
    RegisterHotKey(nullptr, HK_QUIT,       MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q');
    RegisterHotKey(nullptr, HK_AUTO_GG,    MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'G');
    RegisterHotKey(nullptr, HK_WHAT_SAVE,  MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W');
    RegisterHotKey(nullptr, HK_NICE_SHOT,  MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'N');
    RegisterHotKey(nullptr, HK_REQUEUE,    MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'U');
    RegisterHotKey(nullptr, HK_CAM_APPLY,  MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'C');
    RegisterHotKey(nullptr, HK_OBS_CLIP,   MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'S');
    RegisterHotKey(nullptr, HK_TOG_TRAIL,  MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F1);
    RegisterHotKey(nullptr, HK_TOG_AUTOQ,  MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F2);
    RegisterHotKey(nullptr, HK_TOG_REVEAL, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F3);
}

std::wstring make_status(const GameProcess& gp, const EacStatus& eac,
                         bool pad_connected, bool pad_armed) {
    std::wostringstream o;
    o << L"rl: ";
    if (gp.pid) o << L"pid=" << gp.pid << (gp.fullscreen ? L" fs" : L" win");
    else        o << L"not running";
    o << L"  |  pad: ";
    if (!pad_connected)      o << L"disconnected";
    else if (!pad_armed)     o << L"safe-disarmed";
    else                     o << L"armed";
    o << L"  |  eac: " << (eac.armed() ? eac.detail : std::wstring(L"clear"));
    return o.str();
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Settings settings;

    ViGEmPad pad;
    bool pad_connected = pad.connect();
    MacroPlayer macros(pad);

    plugins::AutoQueue        auto_queue;
    plugins::BallTrail        ball_trail;
    plugins::ObsReplay        obs;
    obs.configure(L"127.0.0.1", 4455, "");
    LogWatcher                log_watch;
    plugins::MmrReveal        mmr;
    {
        PWSTR appdata = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0,
                                           nullptr, &appdata))) {
            std::wstring bd = std::wstring(appdata) + L"\\bonezmod\\ranks";
            CreateDirectoryW((std::wstring(appdata) + L"\\bonezmod").c_str(), nullptr);
            CreateDirectoryW(bd.c_str(), nullptr);
            mmr.set_badge_dir(bd);
            CoTaskMemFree(appdata);
        }
    }
    mmr.attach(log_watch);
    std::atomic<bool> reveal_on { true };
    std::atomic<bool> trail_on { true };
    std::atomic<bool> autoq_on { true };

    Overlay overlay;
    DxgiCapture capture;

    // Initial placement: whole primary monitor until we see RL.
    HMONITOR primary = MonitorFromPoint({0,0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(primary, &mi);
    overlay.init(mi.rcMonitor);
    capture.init(primary);
    overlay.set_click_through(true);

    register_hotkeys();

    HudState hud;
    FrameFeatures feats;
    RECT last_game_rect = mi.rcMonitor;
    HMONITOR last_monitor = primary;
    auto last_scan = std::chrono::steady_clock::now();

    overlay.set_paint([&](Overlay::PaintCtx& c){
        paint_hud(c, hud);
        if (trail_on) ball_trail.draw(c, last_game_rect);
        if (reveal_on) mmr.draw(c);
    });

    for (;;) {
        // Rescan for RL + EAC ~4 Hz.
        auto now = std::chrono::steady_clock::now();
        if (now - last_scan > std::chrono::milliseconds(250)) {
            last_scan = now;
            GameProcess gp;
            bool have_rl = find_rocket_league(gp);
            EacStatus eac = check_eac(have_rl ? gp.pid : 0);

            bool pad_should_arm = settings.vpad_arm && !eac.armed();
            hud.eac_warning = eac.armed();

            if (have_rl) {
                overlay.reposition(gp.client_rect);
                last_game_rect = gp.client_rect;
                HMONITOR mon = MonitorFromWindow(gp.hwnd, MONITOR_DEFAULTTONEAREST);
                if (mon != last_monitor) {
                    capture.shutdown();
                    capture.init(mon);
                    last_monitor = mon;
                }
            }

            hud.status_line = make_status(gp, eac, pad_connected, pad_should_arm);
            // Refresh rank line lazily; skipped if not configured.
            if (hud.rank_line.empty() && !settings.tracker_handle.empty()) {
                auto p = fetch_profile(settings.tracker_platform,
                                       settings.tracker_handle);
                if (p) {
                    std::wstring line = L"rank: ";
                    for (auto& r : p->ranks) {
                        line += std::wstring(r.playlist.begin(), r.playlist.end());
                        line += L"=";
                        line += std::wstring(r.tier.begin(), r.tier.end());
                        line += L" ";
                    }
                    hud.rank_line = line;
                }
            }

            // Vision pass (best effort, non-blocking).
            if (settings.vision_enabled) {
                DxgiCapture::Frame frame;
                if (capture.acquire(0, frame)) {
                    feats = analyze(frame, last_game_rect);
                    if (feats.ball) {
                        hud.ball_x = feats.ball->x - last_game_rect.left;
                        hud.ball_y = feats.ball->y - last_game_rect.top;
                        hud.ball_r = feats.ball->radius;
                    } else {
                        hud.ball_x = hud.ball_y = hud.ball_r = -1;
                    }
                    if (feats.boost) hud.boost = feats.boost->value;
                    if (trail_on) ball_trail.push(feats);
                    if (autoq_on && pad_connected && settings.vpad_arm
                                 && !hud.eac_warning)
                        auto_queue.tick(frame, last_game_rect, macros);
                }
            }
        }

        // Drain hotkey messages before render pumps its own queue.
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) {
                bool pad_ok = pad_connected && settings.vpad_arm
                           && !hud.eac_warning;
                switch (msg.wParam) {
                    case HK_RESET_FULL:
                        if (pad_ok) plugins::full_freeplay_reset(macros);
                        break;
                    case HK_RESET_SHOT:
                        if (pad_ok) plugins::freeplay_shot_reset(macros);
                        break;
                    case HK_TOGGLE_OVR:
                        settings.overlay_enabled = !settings.overlay_enabled;
                        ShowWindow(GetActiveWindow(),
                                   settings.overlay_enabled ? SW_SHOWNA : SW_HIDE);
                        break;
                    case HK_TOGGLE_CT: {
                        bool ct = !settings.click_through;
                        settings.click_through = ct;
                        overlay.set_click_through(ct);
                        break;
                    }
                    case HK_QUIT:
                        goto done;
                    case HK_AUTO_GG:    if (pad_ok) plugins::auto_gg(macros); break;
                    case HK_WHAT_SAVE:  if (pad_ok) plugins::what_a_save(macros); break;
                    case HK_NICE_SHOT:  if (pad_ok) plugins::nice_shot(macros); break;
                    case HK_REQUEUE:    if (pad_ok) plugins::requeue(macros); break;
                    case HK_CAM_APPLY:  if (pad_ok) plugins::apply(macros, plugins::kSquishy, true); break;
                    case HK_OBS_CLIP:   std::thread([&]{ obs.save_replay_buffer(); }).detach(); break;
                    case HK_TOG_TRAIL:  trail_on = !trail_on; break;
                    case HK_TOG_AUTOQ:  autoq_on = !autoq_on; break;
                    case HK_TOG_REVEAL: reveal_on = !reveal_on; break;
                }
            } else if (msg.message == WM_QUIT) {
                goto done;
            } else {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        if (!overlay.pump_and_render()) break;

        // ~120 Hz cap
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
done:
    UnregisterHotKey(nullptr, HK_RESET_FULL);
    UnregisterHotKey(nullptr, HK_RESET_SHOT);
    UnregisterHotKey(nullptr, HK_TOGGLE_OVR);
    UnregisterHotKey(nullptr, HK_TOGGLE_CT);
    UnregisterHotKey(nullptr, HK_QUIT);
    UnregisterHotKey(nullptr, HK_AUTO_GG);
    UnregisterHotKey(nullptr, HK_WHAT_SAVE);
    UnregisterHotKey(nullptr, HK_NICE_SHOT);
    UnregisterHotKey(nullptr, HK_REQUEUE);
    UnregisterHotKey(nullptr, HK_CAM_APPLY);
    UnregisterHotKey(nullptr, HK_OBS_CLIP);
    UnregisterHotKey(nullptr, HK_TOG_TRAIL);
    UnregisterHotKey(nullptr, HK_TOG_AUTOQ);
    UnregisterHotKey(nullptr, HK_TOG_REVEAL);
    log_watch.stop();

    capture.shutdown();
    overlay.shutdown();
    pad.disconnect();
    return 0;
}
