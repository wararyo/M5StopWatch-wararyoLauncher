#pragma once
#include "app/ScreenId.h"
#include "features/launcher/AppListModel.h"
#include "features/settings/SettingsModel.h"
#include "features/external/ExternalModel.h"
#include "features/stopwatch/StopwatchModel.h"
#include "ui/rendering/Viewport.h"
namespace launcher {
// What a frame shows happening, for the render metrics. The app decides it
// from which screen owns the motion, so the metrics never infer it from one
// feature's model (a hidden list, say).
enum class FrameActivity : uint8_t {
    Single,          // Anything else: a press, a clock tick, a notice.
    Transition,      // Home to list and back.
    LauncherScroll,  // The app list dragged or coasting.
    SettingsScroll,  // The settings menu dragged, coasting or aligning.
    Stopwatch,       // A running measurement.
    SettingsSingle,  // Settings at rest: a selection or a value changing.
};
// One frame, composed by the app from each feature's own model. It holds only
// state: where each part is drawn follows from it in one place
// (app/FrameComposer.h), so a caller cannot leave a region out of date.
struct FrameModel {
    ScreenId screen=ScreenId::Home;
    Viewport viewport{};
    uint32_t homeCount=0;
    AppListModel launcher{};
    SettingsModel settings{};
    ExternalModel external{};
    StopwatchModel stopwatch{};
    const char* toast=nullptr;
    // The statistics overlay, an application-wide runtime setting.
    bool stats=false;
    FrameActivity activity=FrameActivity::Single;
};
}
