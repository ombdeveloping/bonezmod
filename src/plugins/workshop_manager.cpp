#include "plugins/workshop_manager.h"

#include <windows.h>
#include <filesystem>
namespace fs = std::filesystem;

namespace bonez::plugins {

void WorkshopManager::configure(const std::wstring& lib,
                                const std::wstring& mods,
                                const std::wstring& target) {
    lib_ = lib; mods_ = mods; target_ = target;
    std::error_code ec;
    fs::create_directories(fs::path(mods_), ec);
}

std::wstring WorkshopManager::active_path() const {
    return mods_ + L"\\" + target_ + L".udk";
}

std::vector<WorkshopMap> WorkshopManager::list() const {
    std::vector<WorkshopMap> out;
    if (lib_.empty()) return out;
    std::error_code ec;
    if (!fs::exists(lib_, ec)) return out;
    auto active = active_path();
    uintmax_t active_size = fs::exists(active, ec) ? fs::file_size(active, ec) : 0;
    for (auto& e : fs::directory_iterator(lib_, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        if (_wcsicmp(e.path().extension().c_str(), L".udk") != 0) continue;
        WorkshopMap m;
        m.path = e.path().wstring();
        m.name = e.path().stem().wstring();
        // Heuristic active-check: same size as the file currently deployed.
        m.active = (active_size > 0) &&
                   (fs::file_size(e.path(), ec) == active_size);
        out.push_back(std::move(m));
    }
    return out;
}

bool WorkshopManager::activate(const std::wstring& stem) {
    if (lib_.empty() || mods_.empty() || target_.empty()) return false;
    std::wstring src = lib_ + L"\\" + stem + L".udk";
    std::error_code ec;
    if (!fs::exists(src, ec)) return false;
    fs::copy_file(src, active_path(),
                  fs::copy_options::overwrite_existing, ec);
    return !ec;
}

bool WorkshopManager::deactivate() {
    std::error_code ec;
    fs::remove(active_path(), ec);
    return !ec;
}

} // namespace bonez::plugins
