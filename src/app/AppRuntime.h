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
    void bindSettings(SettingsStore& store,TimeService& time) { screens_.bind(&store,&time); }
    void bindSlots(SlotService& slots) { slots_=&slots; screens_.bindSlots(&slots); }
    void setInfo(const char* name,const char* version,const char* idf) {
        screens_.setInfo(name,version,idf);
    }
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
    SlotService* slots_=nullptr;
    InputController input_;
    ScreenManager screens_;
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
