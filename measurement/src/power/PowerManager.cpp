#include "PowerManager.h"
#include <M5Unified.h>

namespace power {

void PowerManager::begin(uint32_t nowMs) { lastActivityMs_ = nowMs; }

bool PowerManager::update(uint32_t nowMs, bool activity, bool) {
    bool changed = false;
    if (activity) {
        lastActivityMs_ = nowMs;
        if (screenOff_) {
            M5.Display.wakeup();
            M5.Display.setBrightness(90);
            screenOff_ = false;
            changed = true;
        }
    } else if (!screenOff_ && nowMs - lastActivityMs_ >= ScreenOffAfterMs) {
        M5.Display.setBrightness(0);
        M5.Display.sleep();
        screenOff_ = true;
        changed = true;
    }
    return changed;
}

State PowerManager::state(bool usbConnected) const {
    if (screenOff_) return State::ScreenOff;
    if (usbConnected) return State::UsbActive;
    return State::Active;
}

}  // namespace power

