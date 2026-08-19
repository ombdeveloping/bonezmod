#pragma once
#include "input/macro.h"
#include <string>

namespace bonez::plugins {

// Launcher-side helper: types a training pack code by streaming characters
// through the virtual pad's on-screen-keyboard flow. Slow but external.
// If your game uses the physical keyboard for the code box (common on PC),
// use the SendInput path via `type_pack_code_keyboard` instead.
void type_pack_code_vpad(MacroPlayer& mp, const std::string& code);
void type_pack_code_keyboard(const std::string& code);

// Rotate to next / previous shot in the currently loaded pack.
void next_shot(MacroPlayer& mp);
void prev_shot(MacroPlayer& mp);

} // namespace bonez::plugins
