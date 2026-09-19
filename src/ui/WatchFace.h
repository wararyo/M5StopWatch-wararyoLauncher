#pragma once

#include <ctime>
#include "Element.h"

namespace ui {

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
    /// failed to build a cache should return true and fall back to painting
    /// everything each frame.
    virtual bool begin(Gfx& gfx) { (void)gfx; return true; }

    /// Registers this face's elements for one frame, with their boxes at the
    /// given content origin. Every call must be followed by paint() with the
    /// same gfx before the next plan(): a face may keep the laid-out strings
    /// and boxes between the two rather than computing them twice.
    virtual void plan(FramePlan& frame, Gfx& gfx, const WatchData& data, int yOffset) = 0;

    /// Paints the elements the resolved plan asked for.
    virtual void paint(Gfx& gfx, const FramePlan& frame) = 0;

    virtual uint32_t nextUpdateDelayMs(const WatchData& data) const = 0;
};

}  // namespace ui
