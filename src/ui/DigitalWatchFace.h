#pragma once
#include "WatchFace.h"

namespace ui {

class DigitalWatchFace final : public WatchFace {
public:
    bool begin(Gfx& gfx) override;
    void plan(FramePlan& frame, Gfx& gfx, const WatchData& data, int yOffset) override;
    void paint(Gfx& gfx, const FramePlan& frame) override;
    uint32_t nextUpdateDelayMs(const WatchData& data) const override;

private:
    void paintTime(Gfx& gfx);

    /// The large HH:MM glyphs are by far the most expensive thing on this face
    /// and they only change once a minute, so they are rasterised once into an
    /// internal-SRAM sprite and blitted afterwards. Rasterising into internal
    /// SRAM and blitting is much cheaper than scaling glyphs straight into the
    /// PSRAM framebuffer, and it also makes a scroll cheap: the glyphs move
    /// without being rasterised again.
    M5Canvas timeText_;
    char cachedTime_[8] = {};
    bool timeTextReady_ = false;

    // Laid out top to bottom, deliberately non-overlapping within the face.
    Element battery_, date_, time_, dots_, appsLabel_;

    // Filled by plan(), consumed by paint().
    char batteryText_[16] = {};
    char dateText_[32] = {};
    char timeString_[8] = {};
    Rect timeBox_{};  // unclipped, so the sprite lands at the right origin
    int originY_ = 0;
    int handles_[5] = {-1, -1, -1, -1, -1};
};

}  // namespace ui
