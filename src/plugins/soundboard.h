#pragma once
#include <string>
#include <unordered_map>

namespace bonez::plugins {

// Fire-and-forget WAV playback bound to hotkey slot IDs. Uses PlaySound
// with SND_ASYNC | SND_FILENAME so it never blocks the render loop.
// Purely external audio — plays through the user's default output.
class Soundboard {
public:
    void bind(int slot, const std::wstring& wav_path);
    void unbind(int slot);
    bool play(int slot);           // returns false if slot unbound
    void stop_all();

private:
    std::unordered_map<int, std::wstring> slots_;
};

} // namespace bonez::plugins
