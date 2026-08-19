#pragma once
#include "input/vigem_pad.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>

namespace bonez {

// Timed pad-state script. Each Step lasts `hold` and holds a PadState.
struct Step {
    PadState state;
    std::chrono::milliseconds hold{ 50 };
};

// Runs macros on a worker thread. One at a time; queueing a new macro
// preempts the current one.
class MacroPlayer {
public:
    explicit MacroPlayer(ViGEmPad& pad);
    ~MacroPlayer();

    // Enqueue and play immediately (preempts current).
    void play(std::vector<Step> steps);
    void cancel();
    bool busy() const { return busy_.load(); }

private:
    void worker();

    ViGEmPad&                pad_;
    std::thread              th_;
    std::mutex               m_;
    std::vector<Step>        pending_;
    std::atomic<bool>        stop_{false};
    std::atomic<bool>        preempt_{false};
    std::atomic<bool>        busy_{false};
    std::condition_variable  cv_;
};

// --- Prebuilt macro library --------------------------------------------
namespace macros {

std::vector<Step> kickoff_freeplay_reset(); // exit-to-freeplay flow
std::vector<Step> ball_cam_toggle();
std::vector<Step> quick_chat(int slot /*1..4*/, int idx /*1..4*/);
std::vector<Step> pause_menu();
std::vector<Step> shot_reset_training(); // press Backspace-equivalent (Back)

} // namespace macros
} // namespace bonez
