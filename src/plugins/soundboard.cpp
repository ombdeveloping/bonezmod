#include "plugins/soundboard.h"

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace bonez::plugins {

void Soundboard::bind(int slot, const std::wstring& p) { slots_[slot] = p; }
void Soundboard::unbind(int slot) { slots_.erase(slot); }

bool Soundboard::play(int slot) {
    auto it = slots_.find(slot);
    if (it == slots_.end()) return false;
    return PlaySoundW(it->second.c_str(), nullptr,
                      SND_ASYNC | SND_FILENAME | SND_NODEFAULT) != FALSE;
}

void Soundboard::stop_all() {
    PlaySoundW(nullptr, nullptr, 0);
}

} // namespace bonez::plugins
