#pragma once
#include "input/InputController.h"
#include "multifirm/SlotCatalog.h"
#include "services/Stopwatch.h"
#include "storage/Settings.h"
#include <ctime>
namespace launcher {
enum class ScreenId { Home, AppList, Settings, External, Stopwatch };
// Order matters: a menu cursor of 0..3 maps onto the view that follows Menu.
enum class SettingsView : uint8_t { Menu, DateTime, Brightness, ScreenOff, Info };
struct SettingsModel {
    SettingsView view=SettingsView::Menu;
    // Cursor runs over the fields and then the buttons. `editing` is the
    // physical-button mode only: touch changes a value without entering it.
    int cursor=0;
    bool editing=false;
    // DateTime: year, month, day, hour, minute. Brightness: the level itself.
    // ScreenOff: an index into ScreenOffChoices, which is what A cycles.
    int fields[5]{};
    const char* lines[3]{}; // Info text, captured once at startup.
};
// Browsing is the only phase that takes input. BootCommitting is the one-way
// stretch of plan.md 8.2: the frame is painted, then the API is called once.
enum class ExternalPhase : uint8_t { Browsing, BootCommitting, BootFailed };
struct ExternalModel {
    int slot=1;
    SlotStatus status=SlotStatus::Scanning;
    ExternalPhase phase=ExternalPhase::Browsing;
    int cursor=0;
    // Borrowed from the catalog the screen manager holds; stable for the frame.
    const char* name=nullptr;
    const char* version=nullptr;
    const char* message=nullptr; // Boot failure reason.
    int32_t error=0;
};
struct ScreenModel {
    ScreenId screen=ScreenId::Home;
    int width=468,height=468,selection=0;
    uint32_t homeCount=0;
    float transition=0,scroll=0;
    bool dragging=false,animating=false;
    const char* names[5]{}; // optional stable service/diagnostic labels
    bool rowDimmed[5]{};    // Slot cannot be launched: name shown in grey.
    const char* toast=nullptr;
    SettingsModel settings{};
    ExternalModel external{};
    StopwatchModel stopwatch{};
    // Frame statistics overlay. Enabled from the settings information view and
    // never saved, so every boot starts without it.
    bool stats=false;
    int brightness=Settings{}.brightness;   // Effective: preview while editing.
    int screenOffSec=Settings{}.screenOffSec; // Saved only; never previewed.
};
struct WatchData {
    std::tm localTime{}; // JST, supplied by the service; no conversion in UI
    bool timeValid=false;
    int batteryPercent=-1;
    bool charging=false;
    TimeUs subsecondUs=0;
};
class DisplayDataSource {
public:
    virtual ~DisplayDataSource()=default;
    virtual WatchData sample(TimeUs) { return {}; }
    virtual TimeUs nextUpdate(TimeUs) const { return INT64_MAX; }
};
class RenderPort {
public:
    virtual ~RenderPort()=default;
    virtual void invalidate()=0;
    virtual void draw(const ScreenModel&,const WatchData&)=0;
    virtual TimeUs nextUpdate(TimeUs now,const WatchData& data) const=0;
};
inline TimeUs nextMinute(TimeUs now,const WatchData& data) {
    if (!data.timeValid || data.localTime.tm_sec<0 || data.localTime.tm_sec>59 ||
        data.subsecondUs<0 || data.subsecondUs>=1000000) return INT64_MAX;
    return now+(60-data.localTime.tm_sec)*1000000LL-data.subsecondUs;
}
}
