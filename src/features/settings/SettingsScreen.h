#pragma once
#include "host/Screen.h"
#include "services/TimeService.h"
#include "storage/SettingsStore.h"
#include "features/settings/SettingsLayout.h"
#include "ui/list/ListController.h"
#include "features/settings/SettingsMenu.h"
#include <array>
namespace launcher {
class SettingsScreen final : public Screen {
public:
    SettingsScreen() {
        menu_.resize({width_,height_});
        // Input reads only the ids, which never change; labels are drawn from
        // the frame's model instead.
        menu_.setRows(buildSettingsMenuRows(menuRows_));
    }
    // The menu borrows menuRows_, so a copy would point into the original.
    SettingsScreen(const SettingsScreen&)=delete;
    SettingsScreen& operator=(const SettingsScreen&)=delete;
    void resize(int width,int height) override {
        width_=width; height_=height; menu_.resize({width,height});
    }
    void bind(SettingsStore* store,TimeService* time) { store_=store; time_=time; }
    void setInfo(const char* name,const char* version,const char* idf) {
        model_.lines[0]=name; model_.lines[1]=version; model_.lines[2]=idf;
    }
    bool available() const override { return store_ && time_; }
    // A new visit starts at the top, wherever the last one scrolled to.
    void enter(TimeUs) override { menu_.reset(); openView(SettingsView::Menu); }
    // Leaving drops the unsaved edit, and with it the brightness preview,
    // because the preview is derived from the open view rather than stored.
    // The menu stops too, so nothing of settings keeps a deadline.
    void exit() override { menu_.reset(); openView(SettingsView::Menu); }
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    // Only the menu moves on its own, and only while it scrolls.
    TimeUs nextUpdate() const override {
        return model_.view==SettingsView::Menu ? menu_.nextUpdate() : INT64_MAX;
    }
    bool tick(TimeUs now) override { return model_.view==SettingsView::Menu && menu_.update(now); }
    bool active() const override { return model_.view==SettingsView::Menu && menu_.active(); }
    SettingsModel model() const {
        auto copy=model_;
        copy.menu=menu_.state();
        copy.savedBrightness=store_ ? store_->get().brightness : Settings{}.brightness;
        copy.savedScreenOffSec=store_ ? store_->get().screenOffSec : Settings{}.screenOffSec;
        return copy;
    }
    int brightness() const;
    int screenOffSec() const;
private:
    ScreenOutcome handleMenu(const Events& e,TimeUs now);
    void openItem(RowId id,ScreenOutcome& out);
    void openView(SettingsView view);
    void step(int delta);
    // The notice to show, if any. Information's action only asks for the
    // statistics overlay in `out`: the setting is the application's.
    const char* confirm(ScreenOutcome& out);
    SettingsStore* store_=nullptr;
    TimeService* time_=nullptr;
    SettingsModel model_{};
    int width_=468,height_=468;
    // The menu's selection and scroll live here alone, apart from the editor
    // cursor, so an editor's round trip leaves them exactly where they were.
    ListController menu_;
    std::array<ListRow,SettingsMenuCount> menuRows_{};
};
}
