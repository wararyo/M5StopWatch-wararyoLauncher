#pragma once

#include <M5Unified.h>
#include "app/LauncherApp.h"
#include "DigitalWatchFace.h"

namespace ui {

/// Draws straight into the panel, repainting only what changed.
///
/// On M5StopWatch M5GFX installs a Panel_AMOLED_Framebuffer, so M5.Display is
/// already backed by a full-screen PSRAM framebuffer with dirty-rect tracking.
/// There is therefore no intermediate sprite, and no per-frame clear either:
/// the framebuffer keeps last frame's pixels, FramePlan erases and repaints
/// only the elements that changed, and endWrite() sends just the union of those
/// boxes over QSPI. A frame in which nothing changed costs nothing at all.
class Renderer {
public:
    explicit Renderer(M5GFX& display);
    bool begin();

    /// Paints the frame and returns whether anything was actually painted.
    bool draw(const app::State& state, const WatchData& watch);

    /// Forces the next frame to clear and repaint everything. Needed whenever
    /// the panel's contents may no longer match what the elements believe they
    /// painted, such as after waking the display.
    void invalidate() { repaintAll_ = true; }

private:
    void planAppList(const app::State& state, int yOffset);
    void paintAppList(const app::State& state);
    void drawToast(const char* toast);

    M5GFX& display_;
    DigitalWatchFace watchFace_;
    FramePlan frame_;

    // The three rows are tracked as one element: they move together, and a tall
    // box costs no more to flush than three boxes plus the gaps between them.
    Element listItems_;
    Element listHint_;
    int itemsRight_ = 0;

    // Filled by planAppList(), consumed by paintAppList().
    int listOriginY_ = 0;
    int firstRowY_ = 0;
    int rowsHandle_ = -1;
    int hintHandle_ = -1;

    const char* previousToast_ = nullptr;
    bool repaintAll_ = true;
};

}  // namespace ui
