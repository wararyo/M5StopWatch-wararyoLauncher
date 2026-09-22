#pragma once
#include "apps/AppScreen.h"
#include "services/TimeService.h"
#include "storage/SettingsStore.h"
#include "ui/SettingsLayout.h"
namespace launcher {
class SettingsScreen final : public AppScreen {
public:
    void resize(int width,int height) override { width_=width; height_=height; }
    void bind(SettingsStore* store,TimeService* time) { store_=store; time_=time; }
    void setInfo(const char* name,const char* version,const char* idf) {
        model_.lines[0]=name; model_.lines[1]=version; model_.lines[2]=idf;
    }
    bool available() const override { return store_ && time_; }
    void enter(TimeUs) override { menuCursor_=0; openView(SettingsView::Menu); }
    // Leaving drops the unsaved edit, and with it the brightness preview,
    // because the preview is derived from the open view rather than stored.
    void exit() override { openView(SettingsView::Menu); }
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    const SettingsModel& model() const { return model_; }
    int brightness() const;
    int screenOffSec() const;
private:
    ScreenModel layoutModel() const;
    void openView(SettingsView view);
    void step(int delta);
    void activate(ScreenOutcome& out);
    const char* confirm();
    SettingsStore* store_=nullptr;
    TimeService* time_=nullptr;
    SettingsModel model_{};
    int menuCursor_=0;
    int width_=468,height_=468;
};
}
