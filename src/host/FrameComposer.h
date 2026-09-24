#pragma once
#include "host/FrameModel.h"
#include "features/launcher/AppListLayout.h"
#include <cstdint>
namespace launcher {
// The layers of a frame, back to front. The statistics chip is not among them:
// it is painted after all of them, outside the plan (ui/overlays/StatsOverlay.h).
enum class FrameLayer : uint8_t { Home, AppList, Settings, External, Stopwatch, Toast };
inline constexpr FrameLayer FrameOrder[]={
    FrameLayer::Home,FrameLayer::AppList,FrameLayer::Settings,
    FrameLayer::External,FrameLayer::Stopwatch,FrameLayer::Toast};
inline constexpr int FrameLayerCount=int(sizeof(FrameOrder)/sizeof(FrameOrder[0]));
// Where each part of a frame goes, derived from the frame model alone. The
// renderer draws from it and the runtime takes the clock's deadline from it,
// so the clock that is kept up to date is the clock that is drawn.
struct FrameComposition {
    Viewport viewport{};
    // The clock. Its clip is empty whenever none of it is on screen.
    DrawRegion home{};
    // Which layers show. A hidden list still registers its slots empty, so it
    // erases what it drew; a closed screen registers nothing at all.
    bool list=false,settings=false,external=false,stopwatch=false;
    // A different screen than the frame before, set by FrameComposer. Layers
    // that planned nothing then have no history to erase with, and layers that
    // stop planning leave boxes nobody registers: the frame is repainted in
    // full. A change inside one screen (settings' menu and editors) is that
    // layer's own to repaint.
    bool changed=false;
    bool clockVisible() const { return !home.clip.empty(); }
};
inline FrameComposition composeFrame(const FrameModel& m) {
    FrameComposition c;
    c.viewport=m.viewport;
    const bool launcher=m.screen==ScreenId::Home || m.screen==ScreenId::AppList;
    c.home=launcherHomeRegion(m.viewport,m.launcher.transition);
    // An open screen covers the clock, wherever the slide was left.
    if (!launcher) c.home.clip={};
    c.list=launcher;
    c.settings=m.screen==ScreenId::Settings;
    c.external=m.screen==ScreenId::External;
    c.stopwatch=m.screen==ScreenId::Stopwatch;
    return c;
}
inline bool clockVisible(const FrameModel& m) { return composeFrame(m).clockVisible(); }
// The composition from one frame to the next.
class FrameComposer {
public:
    FrameComposition compose(const FrameModel& m) {
        auto c=composeFrame(m);
        c.changed=m.screen!=previous_;
        previous_=m.screen;
        return c;
    }
private:
    ScreenId previous_=ScreenId::Home;
};
}
