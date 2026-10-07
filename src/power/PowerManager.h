#pragma once
#include "core/Time.h"
#include "storage/Settings.h"
namespace launcher {
enum class DisplayState { Active, WatchIdle, ScreenOff };
// A failed read is unknown, never 0%, for the same reason a failed VBUS read is
// not zero volts. The watch face shows `--%` rather than an empty battery.
// `chargingKnown` is false when the PMIC could not say whether it charges:
// that is not the same as "not charging" (docs/task10/plan-10-5.md 3).
struct BatteryState { int percent = -1; bool charging = false; bool chargingKnown = false; };
struct UsbState {
    bool vbusValid = false;
    int vbusMv = 0;
    bool dataConnected = false;
    // The charger's own status pin (M5PM1 G2), read with VBUS. A failed read
    // is unknown, not "not charging".
    bool chargeValid = false, charging = false;
    bool powered() const { return vbusValid && vbusMv > 4000; }
};
class PowerManager {
public:
    void begin(TimeUs now) { lastActivity_ = now; state_ = DisplayState::WatchIdle; }
    // Settings owns the value; the default matches the stored default so an
    // unbound or failed store behaves exactly as before.
    void setTimeout(TimeUs microseconds) { timeout_ = microseconds; }
    bool update(TimeUs now, bool activity, bool visibleActive) {
        const auto old = state_;
        if (activity) lastActivity_ = now;
        if (now - since() >= timeout_) state_ = DisplayState::ScreenOff;
        else state_ = activity || visibleActive ? DisplayState::Active : DisplayState::WatchIdle;
        return old != state_;
    }
    bool screenOff() const { return state_ == DisplayState::ScreenOff; }
    DisplayState state() const { return state_; }
    TimeUs deadline() const { return screenOff() ? INT64_MAX : since() + timeout_; }
    // Keeps a lit panel on until `until`, which then counts as the last
    // activity: the timeout runs from there (a timer's alert, docs/task12/plan.md
    // 2.5). 0 drops it, and the timeout runs from the last input again. It
    // does not light a dark panel: update() with activity does.
    void holdUntil(TimeUs until) { hold_ = until; }
    UsbState usb{};
private:
    TimeUs since() const { return lastActivity_ > hold_ ? lastActivity_ : hold_; }
    TimeUs lastActivity_ = 0, hold_ = 0;
    TimeUs timeout_ = TimeUs(Settings{}.screenOffSec) * 1000000;
    DisplayState state_ = DisplayState::WatchIdle;
};
}
