#pragma once
#include "hal/Hal.h"
#include "app/ScreenManager.h"
#include "app/RenderPort.h"
#include "features/home/DisplayDataSource.h"
#include <algorithm>
namespace launcher {
// The single UI task's loop: input, power, deadlines, slot results and when to
// draw. It drives the screens it is lent and owns none of the application's
// state (app/Application.h owns it).
class AppRuntime {
public:
    AppRuntime(Hal& hal, RenderPort& renderer, DisplayDataSource& data, ScreenManager& screens)
        : hal_(hal), renderer_(renderer), data_(data),
          input_(std::max(1, std::min(screens.viewport().width, screens.viewport().height) / 50)),
          screens_(screens) {}
    AppRuntime(const AppRuntime&) = delete;
    AppRuntime& operator=(const AppRuntime&) = delete;
    void bindSlots(SlotService& slots) { slots_=&slots; screens_.bindSlots(&slots); }
    void begin();
    void step();
    void wait();
    const PowerManager& power() const { return power_; }
    FrameModel model() const { return screens_.model(); }
    void dataChanged() { dirty_ = true; } // UI-task service notification
    // Round up in HAL to ticks; even an overrun must give idle a chance.
    static TimeUs waitDelay(TimeUs now, TimeUs deadline) { return std::max<TimeUs>(1000, deadline - now); }
private:
    Hal& hal_;
    RenderPort& renderer_;
    DisplayDataSource& data_;
    SlotService* slots_=nullptr;
    InputController input_;
    ScreenManager& screens_;
    PowerManager power_;
    TimeUs nextInput_ = 0, nextUsb_ = 0;
    // How long input is still read at its period after an interrupt.
    static constexpr TimeUs FollowUs = 100000;
    TimeUs followUntil_ = 0;
    TimeUs nextDisplay_ = INT64_MAX;
    int appliedBrightness_ = -1; // Forced re-apply after every wake.
    bool dirty_ = true;
    bool scanRequested_ = false; // The scan starts behind the first frame.
};
}
