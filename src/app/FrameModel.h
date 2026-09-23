#pragma once
#include "app/ScreenId.h"
#include "features/home/HomeModel.h"
#include "features/launcher/AppListModel.h"
#include "features/settings/SettingsModel.h"
#include "features/external/ExternalModel.h"
#include "features/stopwatch/StopwatchModel.h"
#include "ui/Viewport.h"
#include "ui/ListLayout.h"
#include <algorithm>
namespace launcher {
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
    DrawRegion homeRegion{};
    Viewport viewport() const { return {width,height}; }
};
inline void composeHomeRegion(ScreenModel& m) {
    const int offset=-int(m.transition*m.height);
    m.homeRegion={{m.width,m.height},offset,
                  {0,0,m.width,std::max(0,m.height+offset)}};
}
inline ListGeometry listGeometry(const ScreenModel& m) {
    ListGeometry g; g.width=m.width; g.height=m.height;
    g.transition=m.transition; g.scroll=m.scroll;
    return g;
}
}
