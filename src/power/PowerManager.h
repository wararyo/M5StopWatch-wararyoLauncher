#pragma once
#include "input/InputController.h"
namespace launcher {
enum class DisplayState { Active, WatchIdle, ScreenOff };
// A failed read is unknown, never 0%, for the same reason a failed VBUS read is
// not zero volts. The watch face shows `--%` rather than an empty battery.
struct BatteryState { int percent = -1; bool charging = false; };
struct UsbState {
    bool vbusValid = false;
    int vbusMv = 0;
    bool dataConnected = false;
    bool powered() const { return vbusValid && vbusMv > 4000; }
};
class PowerManager {
public:
    void begin(TimeUs now) { lastActivity_ = now; state_ = DisplayState::WatchIdle; }
    bool update(TimeUs now, bool activity, bool visibleActive) {
        const auto old = state_;
        if (activity) lastActivity_ = now;
        if (now - lastActivity_ >= 30000000) state_ = DisplayState::ScreenOff;
        else state_ = activity || visibleActive ? DisplayState::Active : DisplayState::WatchIdle;
        return old != state_;
    }
    bool screenOff() const { return state_ == DisplayState::ScreenOff; }
    DisplayState state() const { return state_; }
    TimeUs deadline() const { return screenOff() ? INT64_MAX : lastActivity_ + 30000000; }
    UsbState usb{};
private:
    TimeUs lastActivity_ = 0;
    DisplayState state_ = DisplayState::WatchIdle;
};
}
