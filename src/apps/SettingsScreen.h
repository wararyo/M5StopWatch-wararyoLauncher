#pragma once
#include "services/TimeService.h"
#include "storage/SettingsStore.h"
#include "ui/SettingsLayout.h"
namespace launcher {
struct SettingsOutcome {
    bool changed=false,leave=false;
    const char* notice=nullptr; // Shown by ScreenManager as the usual toast.
};
// The first built-in app screen. It is shaped like the contract in plan.md 4.1
// (enter / exit / handle / model / next update) but is not an interface yet:
// the abstraction is worth extracting once the stopwatch makes it two.
class SettingsScreen {
public:
    void resize(int width,int height) { width_=width; height_=height; }
    void bind(SettingsStore* store,TimeService* time) { store_=store; time_=time; }
    void setInfo(const char* name,const char* version,const char* idf) {
        model_.lines[0]=name; model_.lines[1]=version; model_.lines[2]=idf;
    }
    bool available() const { return store_ && time_; }
    void enter() { menuCursor_=0; openView(SettingsView::Menu); }
    // Leaving drops the unsaved edit, and with it the brightness preview,
    // because the preview is derived from the open view rather than stored.
    void exit() { openView(SettingsView::Menu); }
    SettingsOutcome handle(const Events& e,TimeUs now);
    const SettingsModel& model() const { return model_; }
    int brightness() const;
    int screenOffSec() const;
private:
    ScreenModel layoutModel() const;
    void openView(SettingsView view);
    void step(int delta);
    void activate(SettingsOutcome& out);
    const char* confirm();
    SettingsStore* store_=nullptr;
    TimeService* time_=nullptr;
    SettingsModel model_{};
    int menuCursor_=0;
    int width_=468,height_=468;
};
}
