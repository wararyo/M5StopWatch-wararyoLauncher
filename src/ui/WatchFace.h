#pragma once

#include <M5GFX.h>
#include <ctime>

namespace ui {

/// Common base of M5GFX (the panel, which already owns a PSRAM framebuffer)
/// and M5Canvas (a sprite). Watch faces draw through this so they can target
/// either without an intermediate full-screen buffer.
using Gfx = m5gfx::LovyanGFX;

struct WatchData {
    std::tm localTime{};
    bool timeValid = false;
    int batteryPercent = -1;
    bool charging = false;
};

class WatchFace {
public:
    virtual ~WatchFace() = default;
    virtual void draw(Gfx& gfx, const WatchData& data, int yOffset) const = 0;
    virtual uint32_t nextUpdateDelayMs(const WatchData& data) const = 0;
};

}  // namespace ui
