#pragma once
#include <cstdint>
namespace launcher {
enum class SettingsView : uint8_t { Menu, DateTime, Brightness, ScreenOff, Info };
struct SettingsModel {
    SettingsView view=SettingsView::Menu;
    int cursor=0;
    bool editing=false;
    int fields[5]{};
    const char* lines[3]{};
    int savedBrightness=0,savedScreenOffSec=0;
};
}
