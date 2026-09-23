#pragma once
#include "apps/AppScreen.h"
#include "services/TimeService.h"
#include "storage/SettingsStore.h"
#include "ui/SettingsLayout.h"
#include "app/EffectiveSettings.h"
namespace launcher {
class SettingsScreen final : public AppScreen {
public:
    void resize(int width,int height) override { width_=width; height_=height; }
    void bind(SettingsStore* store,TimeService* time,RuntimeSettings* runtime=nullptr) {
        store_=store; time_=time; runtime_=runtime;
    }
    void setInfo(const char* name,const char* version,const char* idf) {
        model_.lines[0]=name; model_.lines[1]=version; model_.lines[2]=idf;
    }
    bool available() const override { return store_ && time_; }
    void enter(TimeUs) override { menuCursor_=0; openView(SettingsView::Menu); }
    // Leaving drops the unsaved edit, and with it the brightness preview,
    // because the preview is derived from the open view rather than stored.
    void exit() override { openView(SettingsView::Menu); }
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    SettingsModel model() const {
        auto copy=model_;
        copy.savedBrightness=store_ ? store_->get().brightness : Settings{}.brightness;
        copy.savedScreenOffSec=store_ ? store_->get().screenOffSec : Settings{}.screenOffSec;
        return copy;
    }
    // One way on purpose: the overlay is a measurement aid, and having no path
    // back to off means no leftover pixels to erase (docs/plan.md 6.3).
    bool stats() const { return runtime_ && runtime_->stats; }
    int brightness() const;
    int screenOffSec() const;
private:
    void openView(SettingsView view);
    void step(int delta);
    void activate(ScreenOutcome& out);
    const char* confirm();
    SettingsStore* store_=nullptr;
    TimeService* time_=nullptr;
    SettingsModel model_{};
    // The application owns this non-persistent choice across screen entries.
    RuntimeSettings* runtime_=nullptr;
    int menuCursor_=0;
    int width_=468,height_=468;
};
}
