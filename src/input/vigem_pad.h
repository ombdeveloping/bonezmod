#pragma once
#include <cstdint>

namespace bonez {

// Xbox 360-shaped input state. Sticks in [-1,1], triggers in [0,1].
struct PadState {
    float lx = 0, ly = 0;
    float rx = 0, ry = 0;
    float lt = 0, rt = 0;
    bool  a = false, b = false, x = false, y = false;
    bool  lb = false, rb = false;
    bool  back = false, start = false;
    bool  ls = false, rs = false;
    bool  dpad_up = false, dpad_down = false, dpad_left = false, dpad_right = false;
};

// Virtual Xbox 360 gamepad via ViGEmBus. From the game's / EAC's point of
// view this is an ordinary connected controller — no injection, no hooking.
class ViGEmPad {
public:
    ~ViGEmPad();
    bool connect();
    void disconnect();
    bool connected() const;
    // Submit current pad state; returns false on driver error.
    bool submit(const PadState& s);

private:
    void* client_ = nullptr;   // PVIGEM_CLIENT (opaque here)
    void* target_ = nullptr;   // PVIGEM_TARGET
};

} // namespace bonez
