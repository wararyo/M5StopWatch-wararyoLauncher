#pragma once
#include "hal/Hal.h"
#include <algorithm>
namespace launcher {
class AppRuntime {
public:
    AppRuntime(Hal& hal, int shortSide) : hal_(hal), input_(std::max(1, shortSide / 50)) {}
    void begin();
    void step();
    void wait();
    const PowerManager& power() const { return power_; }
    ScreenModel model() const { return screens_.model(); }
    // Round up in HAL to ticks; even an overrun must give idle a chance.
    static TimeUs waitDelay(TimeUs now, TimeUs deadline) { return std::max<TimeUs>(1000, deadline - now); }
private:
    Hal& hal_;
    InputController input_;
    ScreenManager screens_;
    PowerManager power_;
    TimeUs nextInput_ = 0, nextUsb_ = 0;
    bool dirty_ = true;
};
}
