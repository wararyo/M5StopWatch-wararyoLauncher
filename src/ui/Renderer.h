#pragma once

#include <M5Unified.h>
#include "app/LauncherApp.h"
#include "DigitalWatchFace.h"

namespace ui {

/// Draws straight into the panel.
///
/// On M5StopWatch M5GFX installs a Panel_AMOLED_Framebuffer, so M5.Display is
/// already backed by a full-screen PSRAM framebuffer with dirty-rect tracking.
/// An extra full-screen sprite would only add a PSRAM-to-PSRAM blit of the
/// whole frame, so the renderer draws into the panel buffer directly and lets
/// endWrite() emit exactly one DMA flush per frame. The panel buffer is the
/// back buffer, so nothing is ever shown half-drawn.
class Renderer {
public:
    explicit Renderer(M5GFX& display);
    bool begin();
    void draw(const app::State& state, const WatchData& watch);

private:
    void drawAppList(const app::State& state, int yOffset);
    M5GFX& display_;
    DigitalWatchFace watchFace_;
};

}  // namespace ui
