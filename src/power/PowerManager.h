#pragma once

#include <cstdint>

namespace power {

enum class State : uint8_t { Active, WatchIdle, ScreenOff, UsbActive };

class PowerManager {
public:
    void begin(uint32_t nowMs);
    bool update(uint32_t nowMs, bool activity, bool usbConnected);
    bool screenOff() const { return screenOff_; }
    State state(bool usbConnected) const;

private:
    static constexpr uint32_t ScreenOffAfterMs = 30000;
    uint32_t lastActivityMs_ = 0;
    bool screenOff_ = false;
};

}  // namespace power

