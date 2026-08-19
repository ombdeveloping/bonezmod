#pragma once
#include <string>

namespace bonez {

struct EacStatus {
    bool service_running   = false;   // "EasyAntiCheat" or "EasyAntiCheat_EOS"
    bool module_in_target  = false;   // EAC module loaded into RocketLeague.exe
    bool launcher_present  = false;   // EasyAntiCheat_launcher.exe running
    std::wstring detail;

    bool armed() const {
        // "Armed" here means: is it dangerous to interact with the game process?
        return service_running || module_in_target || launcher_present;
    }
};

// Snapshot current EAC state. Never opens RocketLeague.exe for read; we only
// enumerate loaded modules via ToolHelp32 snapshots on our own permission set.
EacStatus check_eac(unsigned long rl_pid /*=0 if unknown*/);

} // namespace bonez
