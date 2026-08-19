#pragma once
#include "input/macro.h"

namespace bonez::plugins {

// Quick "shot reset" for free-play training: taps Back to reset the shot,
// waits, then neutralizes. Registered under a hotkey by main.
void freeplay_shot_reset(MacroPlayer& mp);

// Full exit-to-freeplay chain (see macros::kickoff_freeplay_reset()).
void full_freeplay_reset(MacroPlayer& mp);

} // namespace bonez::plugins
