#include "SettingsScreen.h"
#include "i18n/Strings.h"
#include <cstdlib>
namespace launcher {
int SettingsScreen::brightness() const {
    if (!store_) return Settings{}.brightness;
    // The preview is simply the open editor's field, so cancelling, going home
    // or saving all restore the stored value without a separate undo path.
    const auto& m=editor_.model();
    return m.view==SettingsView::Brightness ? m.fields[0] : store_->get().brightness;
}
int SettingsScreen::screenOffSec() const {
    return store_ ? store_->get().screenOffSec : Settings{}.screenOffSec;
}
void SettingsScreen::openView(SettingsView view) {
    // The menu's row and scroll are the list controller's and are not touched
    // here, so saving and cancelling both return where the eye already is.
    editor_.open(view);
    if (!available()) return;
    auto& m=editor_.model();
    if (view==SettingsView::DateTime) {
        dateTimeFields(*time_,m.fields);
    } else if (view==SettingsView::Brightness) {
        m.fields[0]=store_->get().brightness;
    } else if (view==SettingsView::ScreenOff) {
        m.fields[0]=screenOffIndex(store_->get().screenOffSec);
    } else if (view==SettingsView::WatchFace) {
        // A new visit starts at the top: the faces, then back.
        faceList_.setRows(buildSettingsFaceRows(faceRows_,faceCount()));
        faceList_.reset();
    }
}
const char* SettingsScreen::confirm(ScreenOutcome& out) {
    const auto& m=editor_.model();
    const int fields=settingsFieldCount(m.view);
    const int actions=settingsActionCount(m.view);
    const int action=m.cursor-fields;
    // An action stays where it is: the view does not close, and there is
    // nothing to save, so it never reaches the notice paths below. Action 0 is
    // the statistics overlay, which only ever turns on (docs/plan.md 5.4): one
    // way on purpose, since the overlay is a measurement aid and having no path
    // back to off means no leftover pixels to erase (docs/plan.md 6.3).
    if (action>=0 && action<actions) {
        if (action==0) out.enableStats=true;
        return nullptr;
    }
    const bool cancel=settingsButtonCount(m.view)==2 && m.cursor==fields+actions+1;
    if (m.view==SettingsView::Info || cancel) { openView(SettingsView::Menu); return nullptr; }
    if (m.view==SettingsView::DateTime) {
        const SaveResult result=saveDateTime(*time_,m.fields);
        if (result==SaveResult::Saved) openView(SettingsView::Menu);
        return dateTimeNotice(result);
    }
    Settings next=store_->get();
    if (m.view==SettingsView::Brightness) next.brightness=uint8_t(m.fields[0]);
    else next.screenOffSec=ScreenOffChoices[m.fields[0]];
    if (!store_->save(next)) return text::SaveFailed;
    openView(SettingsView::Menu);
    return text::Saved;
}
void SettingsScreen::openItem(RowId id,ScreenOutcome& out) {
    SettingsView view=SettingsView::Menu;
    if (!settingsItemView(id,view)) { out.leave=true; return; }
    // Whatever was still moving arrives now: the editor covers the menu, and
    // the menu comes back on the same row at the same scroll, with no deadline
    // running behind the editor.
    menu_.finish();
    openView(view);
}
const char* SettingsScreen::chooseFace(RowId id) {
    const int index=settingsFaceIndex(id);
    if (id==SettingsFaceBack || !faces_ || index<0 || index>=faceCount()) {
        faceList_.finish(); openView(SettingsView::Menu); return nullptr;
    }
    // The view stays open: the mark moves to the face now shown.
    switch (faces_->chooseFace(faces_->faceAt(index).id)) {
    case FaceChoiceResult::SaveFailed: return text::SaveFailed;
    case FaceChoiceResult::Failed: return text::FaceUnavailable;
    default: return nullptr;
    }
}
// The same order as the launcher's list (ScreenManager::handle), without its
// pull back to the clock: at the top a downward drag only scrolls back. Both
// lists fill the panel at rest.
ScreenOutcome SettingsScreen::handleList(ListController& list,const Events& e,TimeUs now,ListDecision& decision) {
    ScreenOutcome out{};
    if (e.gesture==Gesture::TouchStart) { out.changed=list.touchStart(); return out; }
    if ((e.gesture==Gesture::Tap || e.gesture==Gesture::DragEnd) && list.releaseAfterStop(now)) {
        out.changed=true; return out;
    }
    if (e.gesture==Gesture::Cancel) { list.cancel(now); out.changed=true; return out; }
    if (e.gesture==Gesture::DragStart) {
        if (std::abs(e.totalX)>std::abs(e.totalY)) return out;
        list.dragStart();
    }
    if (e.gesture==Gesture::DragStart || e.gesture==Gesture::DragMove) {
        out.changed=list.dragMove(float(e.totalY)); return out;
    }
    if (e.gesture==Gesture::DragEnd) {
        if (list.state().dragging) { list.dragEnd(-e.velocityY,now); out.changed=true; }
        return out;
    }
    if (e.next) { out.changed=list.next(now); return out; }
    if (e.gesture==Gesture::Tap)
        decision=list.tap(settingsMenuPlacement({width_,height_},list.scroll()),e.x,e.y,now);
    else if (e.decide) decision=list.decide(now);
    out.changed=decision.changed;
    return out;
}
ScreenOutcome SettingsScreen::handle(const Events& e,TimeUs now) {
    ScreenOutcome out{};
    if (!available()) return out;
    if (shownList()) {
        // Only an editor follows B. A press still followed here began in an
        // editor that closed under it (save tapped while B was down), so its
        // release decides nothing in the list either.
        Events in=e;
        auto& hold=editor_.hold();
        if (in.decide && hold.following()) in.decide=hold.release(-1);
        else if (in.holdChanged && in.hold!=Hold::B) hold.end();
        ListDecision decision;
        if (editor_.model().view==SettingsView::Menu) {
            out=handleList(menu_,in,now,decision);
            if (decision.decided) openItem(decision.id,out);
            return out;
        }
        out=handleList(faceList_,in,now,decision);
        if (decision.decided) out.notice=chooseFace(decision.id);
        return out;
    }
    // The editors (docs/task12/plan.md 1.5), whose buttons settings carries
    // out itself.
    return editor_.handle(e,[this](ScreenOutcome& out) { return confirm(out); });
}
bool SettingsScreen::tick(TimeUs now) {
    if (auto* l=shownList()) return l->update(now);
    return editor_.tick(now);
}
}
