#include "plugins/freeplay_reset.h"

namespace bonez::plugins {

void freeplay_shot_reset(MacroPlayer& mp) {
    mp.play(macros::shot_reset_training());
}
void full_freeplay_reset(MacroPlayer& mp) {
    mp.play(macros::kickoff_freeplay_reset());
}

} // namespace bonez::plugins
