#pragma once
#include "ui/list/ListModel.h"
#include <cstdint>
namespace launcher {
enum class SettingsView : uint8_t { Menu, DateTime, Brightness, ScreenOff, Info, WatchFace };
// The faces settings can choose from (docs/task10/plan-10-5.md 5).
inline constexpr int SettingsFaceCapacity=4;
struct SettingsModel {
    SettingsView view=SettingsView::Menu;
    // The top menu's selection and scroll, owned by the screen's own list
    // controller. It survives the editors, which use `cursor` instead.
    ListState menu{};
    // Editor and information views only: the focused field, action or button.
    // A focused field is what B steps; there is no separate editing state
    // (docs/task12/plan.md 1.5).
    int cursor=0;
    int fields[5]{};
    const char* lines[3]{};
    int savedBrightness=0,savedScreenOffSec=0;
    // The watch face list: its own selection and scroll, the faces' names as
    // the clock layer registered them, and which one shows (-1: none).
    ListState faces{};
    int faceCount=0,currentFace=-1;
    const char* faceNames[SettingsFaceCapacity]{};
};
}
