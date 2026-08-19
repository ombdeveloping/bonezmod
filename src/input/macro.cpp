#include "input/macro.h"

#include <condition_variable>

namespace bonez {

MacroPlayer::MacroPlayer(ViGEmPad& pad) : pad_(pad) {
    th_ = std::thread([this]{ worker(); });
}

MacroPlayer::~MacroPlayer() {
    stop_ = true;
    cv_.notify_all();
    if (th_.joinable()) th_.join();
}

void MacroPlayer::play(std::vector<Step> steps) {
    {
        std::lock_guard<std::mutex> lk(m_);
        pending_ = std::move(steps);
        preempt_ = true;
    }
    cv_.notify_all();
}

void MacroPlayer::cancel() {
    preempt_ = true;
    cv_.notify_all();
}

void MacroPlayer::worker() {
    while (!stop_) {
        std::vector<Step> job;
        {
            std::unique_lock<std::mutex> lk(m_);
            cv_.wait(lk, [&]{ return stop_ || !pending_.empty(); });
            if (stop_) return;
            job.swap(pending_);
            preempt_ = false;
        }
        busy_ = true;
        // Neutral state at start.
        pad_.submit(PadState{});
        for (auto& step : job) {
            if (stop_ || preempt_) break;
            pad_.submit(step.state);
            auto until = std::chrono::steady_clock::now() + step.hold;
            while (std::chrono::steady_clock::now() < until) {
                if (stop_ || preempt_) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
            }
        }
        pad_.submit(PadState{});
        busy_ = false;
    }
}

// -----------------------------------------------------------------------

namespace {
using ms = std::chrono::milliseconds;
Step tap(PadState p, int hold_ms = 90) { return { p, ms(hold_ms) }; }
Step wait(int hold_ms) { return { PadState{}, ms(hold_ms) }; }
} // namespace

namespace macros {

std::vector<Step> ball_cam_toggle() {
    PadState p; p.y = true;
    return { tap(p, 80), wait(80) };
}

std::vector<Step> pause_menu() {
    PadState p; p.start = true;
    return { tap(p, 80), wait(120) };
}

std::vector<Step> shot_reset_training() {
    // Rocket League binds "Reset Shot" to Back/Select on controllers.
    PadState p; p.back = true;
    return { tap(p, 80), wait(80) };
}

// Menu navigation macro: pause → Exit → Confirm → Freeplay.
// Timings tuned conservatively; adjust per your menu-animation setting.
std::vector<Step> kickoff_freeplay_reset() {
    std::vector<Step> s;
    // open menu
    PadState pause; pause.start = true;
    s.push_back(tap(pause, 100));
    s.push_back(wait(400));
    // move down to "Exit to main menu"
    PadState down; down.dpad_down = true;
    for (int i = 0; i < 4; ++i) { s.push_back(tap(down, 90)); s.push_back(wait(70)); }
    // confirm
    PadState a; a.a = true;
    s.push_back(tap(a, 90));
    s.push_back(wait(300));
    // confirm exit
    s.push_back(tap(a, 90));
    s.push_back(wait(1800));
    // From main menu → Play → Training → Free Play. Path varies by RL
    // version; treat this as a launch template He can retime.
    s.push_back(tap(a, 90));
    s.push_back(wait(500));
    s.push_back(tap(down, 90));
    s.push_back(wait(120));
    s.push_back(tap(a, 90));
    s.push_back(wait(500));
    s.push_back(tap(a, 90));
    return s;
}

} // namespace macros
} // namespace bonez
