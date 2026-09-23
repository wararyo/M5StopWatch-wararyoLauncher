#pragma once
#include "app/AppRegistry.h"
#include "ui/list/ListModel.h"
namespace launcher {
// One row per registry entry, in registry order.
inline constexpr int AppListCount=static_cast<int>(AppRegistry.size());
struct AppListModel {
    // The shared list's state; the launcher adds only what the list must not know.
    ListState list{};
    // Home to list, 0 to 1. Turned into an offset and a clip for the list.
    float transition=0;
    // A null name keeps the registry's. Both point at storage that outlives
    // the frame (the registry, the manager's slot catalog), never into a copy.
    const char* names[AppListCount]{};
    bool rowDimmed[AppListCount]{};
};
}
