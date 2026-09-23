#pragma once
#include "ui/list/ListModel.h"
#include <cstdint>
namespace launcher {
enum class SettingsView : uint8_t { Menu, DateTime, Brightness, ScreenOff, Info };
struct SettingsModel {
    SettingsView view=SettingsView::Menu;
    // The top menu's selection and scroll, owned by the screen's own list
    // controller. It survives the editors, which use `cursor` instead.
    ListState menu{};
    // Editor and information views only: the focused field, action or button.
    int cursor=0;
    bool editing=false;
    int fields[5]{};
    const char* lines[3]{};
    int savedBrightness=0,savedScreenOffSec=0;
};
}
