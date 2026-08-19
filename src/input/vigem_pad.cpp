#include "input/vigem_pad.h"

#if BONEZ_HAS_VIGEM
  #include <ViGEm/Client.h>
#endif

#include <algorithm>
#include <cmath>

namespace bonez {

#if BONEZ_HAS_VIGEM

static SHORT to_short(float v) {
    v = std::clamp(v, -1.0f, 1.0f);
    // Map [-1,1] to [-32768, 32767]
    int i = (int)std::lround(v * 32767.0f);
    return (SHORT)std::clamp(i, -32768, 32767);
}
static BYTE to_byte(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    return (BYTE)std::lround(v * 255.0f);
}

ViGEmPad::~ViGEmPad() { disconnect(); }

bool ViGEmPad::connect() {
    if (client_) return true;
    auto* c = vigem_alloc();
    if (!c) return false;
    if (!VIGEM_SUCCESS(vigem_connect(c))) { vigem_free(c); return false; }
    auto* t = vigem_target_x360_alloc();
    if (!t) { vigem_disconnect(c); vigem_free(c); return false; }
    if (!VIGEM_SUCCESS(vigem_target_add(c, t))) {
        vigem_target_free(t); vigem_disconnect(c); vigem_free(c); return false;
    }
    client_ = c;
    target_ = t;
    return true;
}

void ViGEmPad::disconnect() {
    auto* c = static_cast<PVIGEM_CLIENT>(client_);
    auto* t = static_cast<PVIGEM_TARGET>(target_);
    if (c && t) { vigem_target_remove(c, t); vigem_target_free(t); }
    if (c)      { vigem_disconnect(c); vigem_free(c); }
    client_ = target_ = nullptr;
}

bool ViGEmPad::connected() const { return client_ && target_; }

bool ViGEmPad::submit(const PadState& s) {
    if (!connected()) return false;
    XUSB_REPORT r{};
    r.sThumbLX = to_short(s.lx);
    r.sThumbLY = to_short(s.ly);
    r.sThumbRX = to_short(s.rx);
    r.sThumbRY = to_short(s.ry);
    r.bLeftTrigger  = to_byte(s.lt);
    r.bRightTrigger = to_byte(s.rt);

    USHORT b = 0;
    if (s.a)          b |= XUSB_GAMEPAD_A;
    if (s.b)          b |= XUSB_GAMEPAD_B;
    if (s.x)          b |= XUSB_GAMEPAD_X;
    if (s.y)          b |= XUSB_GAMEPAD_Y;
    if (s.lb)         b |= XUSB_GAMEPAD_LEFT_SHOULDER;
    if (s.rb)         b |= XUSB_GAMEPAD_RIGHT_SHOULDER;
    if (s.back)       b |= XUSB_GAMEPAD_BACK;
    if (s.start)      b |= XUSB_GAMEPAD_START;
    if (s.ls)         b |= XUSB_GAMEPAD_LEFT_THUMB;
    if (s.rs)         b |= XUSB_GAMEPAD_RIGHT_THUMB;
    if (s.dpad_up)    b |= XUSB_GAMEPAD_DPAD_UP;
    if (s.dpad_down)  b |= XUSB_GAMEPAD_DPAD_DOWN;
    if (s.dpad_left)  b |= XUSB_GAMEPAD_DPAD_LEFT;
    if (s.dpad_right) b |= XUSB_GAMEPAD_DPAD_RIGHT;
    r.wButtons = b;

    return VIGEM_SUCCESS(vigem_target_x360_update(
        static_cast<PVIGEM_CLIENT>(client_),
        static_cast<PVIGEM_TARGET>(target_), r));
}

#else // no ViGEm — build the file with a stub so the app still links.

ViGEmPad::~ViGEmPad()                    {}
bool ViGEmPad::connect()                 { return false; }
void ViGEmPad::disconnect()              {}
bool ViGEmPad::connected() const         { return false; }
bool ViGEmPad::submit(const PadState&)   { return false; }

#endif

} // namespace bonez
