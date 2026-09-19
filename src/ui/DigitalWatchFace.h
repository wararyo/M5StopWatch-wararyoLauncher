#pragma once
#include "WatchFace.h"

namespace ui {

class DigitalWatchFace final : public WatchFace {
public:
    bool begin(Gfx& gfx) override;
    void draw(Gfx& gfx, const WatchData& data, int yOffset) override;
    uint32_t nextUpdateDelayMs(const WatchData& data) const override;

private:
    void drawTime(Gfx& gfx, const char* text, int centerY);

    /// The large HH:MM glyphs are by far the most expensive thing on this face
    /// and they only change once a minute, so they are rasterised once into an
    /// internal-SRAM sprite and blitted afterwards. Rasterising into internal
    /// SRAM and blitting is much cheaper than scaling glyphs straight into the
    /// PSRAM framebuffer every frame.
    M5Canvas timeText_;
    char cachedTime_[8] = {};
    bool timeTextReady_ = false;
};

}  // namespace ui
