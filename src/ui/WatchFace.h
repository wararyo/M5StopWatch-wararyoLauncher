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

    /// Allocates cached resources. Called once, after the display is up.
    /// Returning false means the face cannot draw at all; a face that merely
    /// failed to build a cache should return true and fall back to drawing
    /// everything each frame.
    virtual bool begin(Gfx& gfx) { (void)gfx; return true; }

    /// Draws the face with its content origin at yOffset. A face occupies one
    /// screen height, so the caller skips it entirely once it has scrolled off.
    /// Not const: faces may carry rasterisation caches.
    virtual void draw(Gfx& gfx, const WatchData& data, int yOffset) = 0;

    virtual uint32_t nextUpdateDelayMs(const WatchData& data) const = 0;
};

}  // namespace ui
