#pragma once
#include <string>
#include <vector>

namespace bonez::plugins {

// External workshop map manager. RL loads any .udk placed in
// TAGame\CookedPCConsole\mods\ as a swap for the current map. This
// plugin lists / activates / deactivates .udk files entirely via file
// system operations — no injection.
struct WorkshopMap {
    std::wstring path;     // full path in library dir
    std::wstring name;     // file stem
    bool active = false;   // currently symlinked into mods/
};

class WorkshopManager {
public:
    // library_dir: where you keep your .udk collection.
    // rl_mods_dir: <RL install>\TAGame\CookedPCConsole\mods\
    // target_map:  the vanilla map to override (e.g. L"Labs_Underpass_P")
    void configure(const std::wstring& library_dir,
                   const std::wstring& rl_mods_dir,
                   const std::wstring& target_map);

    std::vector<WorkshopMap> list() const;
    bool activate(const std::wstring& stem);   // copy → mods\<target>.udk
    bool deactivate();                         // delete mods\<target>.udk

private:
    std::wstring lib_, mods_, target_;
    std::wstring active_path() const;
};

} // namespace bonez::plugins
