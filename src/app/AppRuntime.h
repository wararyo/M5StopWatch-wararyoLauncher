#pragma once
#include "hal/Hal.h"
#include "app/ScreenManager.h"
#include <algorithm>
namespace launcher {
class AppRuntime {
public:
    AppRuntime(Hal& hal, RenderPort& renderer, DisplayDataSource& data, int width, int height)
        : hal_(hal), renderer_(renderer), data_(data),
          input_(std::max(1, std::min(width,height) / 50)), screens_(width,height) {}
    void begin();
    void step();
    void wait();
    const PowerManager& power() const { return power_; }
    ScreenModel model() const { return screens_.model(); }
    void dataChanged() { dirty_ = true; } // UI-task service notification
    // Round up in HAL to ticks; even an overrun must give idle a chance.
    static TimeUs waitDelay(TimeUs now, TimeUs deadline) { return std::max<TimeUs>(1000, deadline - now); }
private:
    Hal& hal_;
    RenderPort& renderer_;
    DisplayDataSource& data_;
    InputController input_;
    ScreenManager screens_;
    PowerManager power_;
    TimeUs nextInput_ = 0, nextUsb_ = 0;
    TimeUs nextDisplay_ = INT64_MAX;
    bool dirty_ = true;
};
}
