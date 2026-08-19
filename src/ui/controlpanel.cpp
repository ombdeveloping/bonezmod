#include "ui/controlpanel.h"

// The panel is stubbed unless ImGui is vendored. main() calls it optionally.
namespace bonez {

#if BONEZ_HAS_IMGUI
// Full ImGui window implementation would go here. Left as a hook point
// because vendoring ImGui is a project-integration decision, not code.
bool run_controlpanel_once(Settings&) { return true; }
#else
bool run_controlpanel_once(Settings&) { return true; }
#endif

} // namespace bonez
