#include "plugins/chat.h"

namespace bonez::plugins {

void quick_chat(MacroPlayer& mp, QuickSlot slot, int phrase) {
    mp.play(macros::quick_chat((int)slot, phrase));
}
void auto_gg(MacroPlayer& mp)      { quick_chat(mp, QuickSlot::Compliments, 2); }
void what_a_save(MacroPlayer& mp)  { quick_chat(mp, QuickSlot::Info,        4); }
void nice_shot(MacroPlayer& mp)    { quick_chat(mp, QuickSlot::Compliments, 3); }
void sorry(MacroPlayer& mp)        { quick_chat(mp, QuickSlot::Apologies,   1); }

} // namespace bonez::plugins
