#include "plugins/training_pack.h"

#include <windows.h>
#include <cctype>

namespace bonez::plugins {

namespace {
using ms = std::chrono::milliseconds;
Step tap(PadState p, int hold = 90) { return { p, ms(hold) }; }
Step wait(int hold) { return { PadState{}, ms(hold) }; }
} // namespace

void next_shot(MacroPlayer& mp) {
    PadState p; p.rb = true;
    mp.play({ tap(p, 80), wait(60) });
}
void prev_shot(MacroPlayer& mp) {
    PadState p; p.lb = true;
    mp.play({ tap(p, 80), wait(60) });
}

// Basic vpad OSK stream: assumes the OSK cursor starts at 'A' and moves in
// a 6-column grid. Real layout has letters + digits + symbols; adjust the
// grid layout constants for your locale/build. Kept small on purpose.
void type_pack_code_vpad(MacroPlayer& mp, const std::string& code) {
    const int COLS = 10;
    // Alphabet+digits layout used by the RL OSK, upper row first.
    static const char* rows[] = {
        "1234567890",
        "QWERTYUIOP",
        "ASDFGHJKL-",
        "ZXCVBNM_. ",
    };
    auto find_cell = [&](char ch, int& cx, int& cy) {
        char up = (char)std::toupper((unsigned char)ch);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < COLS; ++x)
                if (rows[y][x] == up) { cx = x; cy = y; return true; }
        return false;
    };

    std::vector<Step> steps;
    int curx = 0, cury = 0;
    for (char ch : code) {
        int tx, ty;
        if (!find_cell(ch, tx, ty)) continue;
        auto step_dir = [&](bool xdir_pos, int n) {
            PadState p;
            (xdir_pos ? p.dpad_right : p.dpad_left) = true;
            for (int i = 0; i < n; ++i) {
                steps.push_back(tap(p, 60));
                steps.push_back(wait(40));
            }
        };
        auto step_vert = [&](bool down, int n) {
            PadState p;
            (down ? p.dpad_down : p.dpad_up) = true;
            for (int i = 0; i < n; ++i) {
                steps.push_back(tap(p, 60));
                steps.push_back(wait(40));
            }
        };
        int dx = tx - curx;
        if (dx > 0) step_dir(true,  dx);
        if (dx < 0) step_dir(false, -dx);
        int dy = ty - cury;
        if (dy > 0) step_vert(true,   dy);
        if (dy < 0) step_vert(false, -dy);
        PadState a; a.a = true;
        steps.push_back(tap(a, 80));
        steps.push_back(wait(60));
        curx = tx; cury = ty;
    }
    mp.play(std::move(steps));
}

void type_pack_code_keyboard(const std::string& code) {
    std::vector<INPUT> in;
    in.reserve(code.size() * 2);
    for (char ch : code) {
        char up = (char)std::toupper((unsigned char)ch);
        SHORT vk = VkKeyScanA(up);
        if (vk == -1) continue;
        BYTE key = (BYTE)(vk & 0xFF);
        INPUT down{}; down.type = INPUT_KEYBOARD; down.ki.wVk = key;
        INPUT upv{};  upv.type  = INPUT_KEYBOARD; upv.ki.wVk  = key;
        upv.ki.dwFlags = KEYEVENTF_KEYUP;
        in.push_back(down);
        in.push_back(upv);
    }
    if (!in.empty()) SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

} // namespace bonez::plugins
