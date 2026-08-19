#include "process_watch.h"

#include <tlhelp32.h>
#include <dwmapi.h>

namespace bonez {

namespace {
struct EnumCtx { DWORD pid; HWND best; };

BOOL CALLBACK enum_windows_cb(HWND hwnd, LPARAM lp) {
    auto* ctx = reinterpret_cast<EnumCtx*>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    // Skip tool/child windows: we want the top-level game window.
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
    RECT r{};
    if (!GetClientRect(hwnd, &r)) return TRUE;
    if ((r.right - r.left) < 400 || (r.bottom - r.top) < 300) return TRUE;
    ctx->best = hwnd;
    return FALSE; // stop
}
} // namespace

bool find_rocket_league(GameProcess& out) {
    out = {};
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"RocketLeague.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!pid) return false;

    EnumCtx ctx{ pid, nullptr };
    EnumWindows(enum_windows_cb, reinterpret_cast<LPARAM>(&ctx));
    if (!ctx.best) return false;

    out.pid  = pid;
    out.hwnd = ctx.best;

    RECT client{};
    GetClientRect(ctx.best, &client);
    POINT tl{ client.left, client.top };
    POINT br{ client.right, client.bottom };
    ClientToScreen(ctx.best, &tl);
    ClientToScreen(ctx.best, &br);
    out.client_rect = { tl.x, tl.y, br.x, br.y };

    // Rough fullscreen check: window covers the monitor.
    HMONITOR mon = MonitorFromWindow(ctx.best, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (GetMonitorInfoW(mon, &mi)) {
        out.fullscreen =
            out.client_rect.left  <= mi.rcMonitor.left  &&
            out.client_rect.top   <= mi.rcMonitor.top   &&
            out.client_rect.right >= mi.rcMonitor.right &&
            out.client_rect.bottom>= mi.rcMonitor.bottom;
    }
    return true;
}

} // namespace bonez
