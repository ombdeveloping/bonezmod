#pragma once
#include "input/macro.h"

namespace bonez::plugins {

// Chat via the standard 4x4 quick-chat wheel. Each slot maps to a d-pad
// direction (Up=info, Left=compliments, Right=reactions, Down=apologies).
// After the slot direction, the second direction picks the phrase 1..4.
enum class QuickSlot { Info = 1, Compliments = 2, Reactions = 3, Apologies = 4 };

void quick_chat(MacroPlayer& mp, QuickSlot slot, int phrase_1_to_4);

// Common shortcuts.
void auto_gg(MacroPlayer& mp);       // "Well played!" (Compliments → 2)
void what_a_save(MacroPlayer& mp);   // Info → 4
void nice_shot(MacroPlayer& mp);     // Compliments → 3
void sorry(MacroPlayer& mp);         // Apologies → 1

} // namespace bonez::plugins
