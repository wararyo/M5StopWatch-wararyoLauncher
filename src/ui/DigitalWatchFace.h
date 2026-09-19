#pragma once
#include "WatchFace.h"

namespace ui {

class DigitalWatchFace final : public WatchFace {
public:
    void draw(Gfx& gfx, const WatchData& data, int yOffset) const override;
    uint32_t nextUpdateDelayMs(const WatchData& data) const override;
};

}  // namespace ui
