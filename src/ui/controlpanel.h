#pragma once
#include <string>
#include <atomic>

namespace bonez {

// Runtime settings used by main + the paint callback + hotkey dispatch.
struct Settings {
    std::atomic<bool> overlay_enabled { true };
    std::atomic<bool> vision_enabled  { true };
    std::atomic<bool> vpad_arm        { true };   // user intent
    std::atomic<bool> click_through   { true };
    std::atomic<int>  boost_show      { 1 };      // 0=hide,1=show
    std::string       tracker_platform{"steam"};
    std::string       tracker_handle;
};

// Optional ImGui-backed panel (compiled out if ImGui isn't vendored).
// Returns false when the user closes the panel window.
bool run_controlpanel_once(Settings& s);

} // namespace bonez
