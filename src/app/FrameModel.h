#pragma once
#include "app/ScreenId.h"
#include "features/home/HomeModel.h"
#include "features/launcher/AppListModel.h"
#include "features/settings/SettingsModel.h"
#include "features/external/ExternalModel.h"
#include "features/stopwatch/StopwatchModel.h"
#include "ui/Viewport.h"
#include <algorithm>
namespace launcher {
// What a frame shows happening, for the render metrics. The app decides it
// from which screen owns the motion, so the metrics never infer it from one
// feature's model (a hidden list, say) and it stays here when the renderer is
// split off (docs/task9/plan-9-3.md 4).
enum class FrameActivity : uint8_t {
    Single,          // Anything else: a press, a clock tick, a notice.
    Transition,      // Home to list and back.
    LauncherScroll,  // The app list dragged or coasting.
    SettingsScroll,  // The settings menu dragged, coasting or aligning.
    Stopwatch,       // A running measurement.
    SettingsSingle,  // Settings at rest: a selection or a value changing.
};
// Composed for one frame by the app. Feature layers receive only their own model.
struct ScreenModel : AppListModel {
    ScreenId screen=ScreenId::Home;
    int width=468,height=468;
    uint32_t homeCount=0;
    const char* toast=nullptr;
    SettingsModel settings{};
    ExternalModel external{};
    StopwatchModel stopwatch{};
    bool stats=false;
    FrameActivity activity=FrameActivity::Single;
    DrawRegion homeRegion{};
    Viewport viewport() const { return {width,height}; }
};
inline void composeHomeRegion(ScreenModel& m) {
    const int offset=-int(m.transition*m.height);
    m.homeRegion={{m.width,m.height},offset,
                  {0,0,m.width,std::max(0,m.height+offset)}};
}
}
