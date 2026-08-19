#include "eac_guard.h"

#include <windows.h>
#include <tlhelp32.h>
#include <wctype.h>
#include <string>
#include <vector>

namespace bonez {

namespace {

bool icontains(const std::wstring& hay, const wchar_t* needle) {
    std::wstring h; h.reserve(hay.size());
    for (wchar_t c : hay) h.push_back((wchar_t)towlower(c));
    std::wstring n = needle;
    for (auto& c : n) c = (wchar_t)towlower(c);
    return h.find(n) != std::wstring::npos;
}

bool service_running(const wchar_t* name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    bool running = false;
    SC_HANDLE svc = OpenServiceW(scm, name, SERVICE_QUERY_STATUS);
    if (svc) {
        SERVICE_STATUS_PROCESS ssp{};
        DWORD needed = 0;
        if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO,
                                 (LPBYTE)&ssp, sizeof(ssp), &needed)) {
            running = ssp.dwCurrentState == SERVICE_RUNNING
                   || ssp.dwCurrentState == SERVICE_START_PENDING;
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return running;
}

bool process_running(const wchar_t* image) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    bool hit = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, image) == 0) { hit = true; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return hit;
}

// Enumerate loaded modules of a process WITHOUT opening it for VM_READ.
// ToolHelp module snapshot uses TH32CS_SNAPMODULE; if it fails (protected
// process, EAC hardening) we treat that itself as a positive signal.
bool eac_module_loaded(DWORD pid, std::wstring& detail) {
    if (pid == 0) return false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        // ERROR_ACCESS_DENIED / ERROR_PARTIAL_COPY on a hardened process is
        // itself strong evidence EAC or similar protection is active.
        if (e == ERROR_ACCESS_DENIED || e == ERROR_PARTIAL_COPY) {
            detail = L"module snapshot denied (protected process)";
            return true;
        }
        return false;
    }
    MODULEENTRY32W me{}; me.dwSize = sizeof(me);
    bool hit = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (icontains(me.szModule, L"easyanticheat") ||
                icontains(me.szModule, L"eac")           ||
                icontains(me.szModule, L"eossdk")) {
                detail = std::wstring(L"module: ") + me.szModule;
                hit = true;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return hit;
}

} // namespace

EacStatus check_eac(DWORD rl_pid) {
    EacStatus s;
    s.service_running =
        service_running(L"EasyAntiCheat")     ||
        service_running(L"EasyAntiCheat_EOS");
    s.launcher_present =
        process_running(L"EasyAntiCheat.exe") ||
        process_running(L"EasyAntiCheat_EOS.exe") ||
        process_running(L"EasyAntiCheat_launcher.exe");

    std::wstring mod_detail;
    s.module_in_target = eac_module_loaded(rl_pid, mod_detail);

    std::wstring d;
    if (s.service_running)  d += L"[service] ";
    if (s.launcher_present) d += L"[launcher] ";
    if (s.module_in_target) { d += L"["; d += mod_detail; d += L"] "; }
    s.detail = d;
    return s;
}

} // namespace bonez
