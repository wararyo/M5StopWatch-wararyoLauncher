#pragma once
#include "host/Screen.h"
#include "services/TimeService.h"
#include "storage/SettingsStore.h"
#include "features/settings/SettingsEditor.h"
#include "ui/list/ListController.h"
#include "features/settings/SettingsMenu.h"
#include "features/home/HomeInteraction.h"
#include <array>
namespace launcher {
class SettingsScreen final : public Screen {
public:
    SettingsScreen() {
        menu_.resize({width_,height_}); faceList_.resize({width_,height_});
        // Input reads only the ids, which never change; labels are drawn from
        // the frame's model instead.
        menu_.setRows(buildSettingsMenuRows(menuRows_));
    }
    // The menu borrows menuRows_, so a copy would point into the original.
    SettingsScreen(const SettingsScreen&)=delete;
    SettingsScreen& operator=(const SettingsScreen&)=delete;
    void resize(int width,int height) override {
        width_=width; height_=height; menu_.resize({width,height}); faceList_.resize({width,height});
        editor_.resize(width,height);
    }
    void bind(SettingsStore* store,TimeService* time) { store_=store; time_=time; }
    // The clock layer, which lists the faces and carries out a choice. Without
    // it the watch face view offers only back.
    void bindFaces(HomeControlPort* faces) { faces_=faces; }
    void setInfo(const char* name,const char* version,const char* idf) {
        auto& lines=editor_.model().lines; lines[0]=name; lines[1]=version; lines[2]=idf;
    }
    bool available() const override { return store_ && time_; }
    // A new visit starts at the top, wherever the last one scrolled to.
    void enter(TimeUs) override { menu_.reset(); faceList_.reset(); openView(SettingsView::Menu); editor_.hold()=HoldRepeat{}; }
    // Leaving drops the unsaved edit, and with it the brightness preview,
    // because the preview is derived from the open view rather than stored.
    // The menu stops too, so nothing of settings keeps a deadline.
    void exit() override { menu_.reset(); faceList_.reset(); openView(SettingsView::Menu); editor_.hold()=HoldRepeat{}; }
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    // The lists move on their own while they scroll; an editor's value steps
    // while B is held on it.
    TimeUs nextUpdate() const override {
        const auto* l=shownList(); return l ? l->nextUpdate() : editor_.nextUpdate();
    }
    bool tick(TimeUs now) override;
    bool active() const override { const auto* l=shownList(); return l && l->active(); }
    SettingsModel model() const {
        SettingsModel copy{};
        static_cast<SettingsEditorModel&>(copy)=editor_.model();
        copy.menu=menu_.state();
        copy.faces=faceList_.state();
        copy.faceCount=faceCount();
        for (int i=0;i<copy.faceCount;++i) copy.faceNames[i]=faces_->faceAt(i).name;
        copy.currentFace=faces_ ? faces_->currentFace() : -1;
        copy.savedBrightness=store_ ? store_->get().brightness : Settings{}.brightness;
        copy.savedScreenOffSec=store_ ? store_->get().screenOffSec : Settings{}.screenOffSec;
        return copy;
    }
    int brightness() const;
    int screenOffSec() const;
private:
    int faceCount() const { return faces_ ? std::clamp(faces_->faceCount(),0,SettingsFaceCapacity) : 0; }
    const ListController* shownList() const {
        const auto view=editor_.model().view;
        return view==SettingsView::Menu ? &menu_ : view==SettingsView::WatchFace ? &faceList_ : nullptr;
    }
    ListController* shownList() { return const_cast<ListController*>(static_cast<const SettingsScreen*>(this)->shownList()); }
    // A list's input, the same for both lists; a decided row comes back in
    // `decision` for the caller to act on.
    ScreenOutcome handleList(ListController& list,const Events& e,TimeUs now,ListDecision& decision);
    void openItem(RowId id,ScreenOutcome& out);
    // A face row decided: the choice is applied and stored at once; back
    // returns to the menu.
    const char* chooseFace(RowId id);
    void openView(SettingsView view);
    // The notice to show, if any. Information's action only asks for the
    // statistics overlay in `out`: the setting is the application's.
    const char* confirm(ScreenOutcome& out);
    SettingsStore* store_=nullptr;
    TimeService* time_=nullptr;
    // The editor and information views, and the view shown (the menu and the
    // watch face choice included).
    SettingsEditor editor_;
    int width_=468,height_=468;
    // The menu's selection and scroll live here alone, apart from the editor
    // cursor, so an editor's round trip leaves them exactly where they were.
    ListController menu_;
    std::array<ListRow,SettingsMenuCount> menuRows_{};
    // The watch face choice: its own list, so the menu keeps its row.
    ListController faceList_;
    std::array<ListRow,SettingsFaceRows> faceRows_{};
    HomeControlPort* faces_=nullptr;
};
}
