#include "SettingsScreen.h"
#include "i18n/Strings.h"
#include <cstdlib>
namespace launcher {
namespace {
int wrap(int value,int low,int high,int delta,int step=1) {
    const int next=value+delta*step;
    if (next>high) return low;
    if (next<low) return high;
    return next;
}
}
int SettingsScreen::brightness() const {
    if (!store_) return Settings{}.brightness;
    // The preview is simply the open editor's field, so cancelling, going home
    // or saving all restore the stored value without a separate undo path.
    return model_.view==SettingsView::Brightness ? model_.fields[0] : store_->get().brightness;
}
int SettingsScreen::screenOffSec() const {
    return store_ ? store_->get().screenOffSec : Settings{}.screenOffSec;
}
void SettingsScreen::openView(SettingsView view) {
    // The menu's row and scroll are the list controller's and are not touched
    // here, so saving and cancelling both return where the eye already is.
    model_.view=view;
    model_.cursor=0;
    // A press held across the change belongs to the view it began in: its
    // release must not decide anything in this one.
    hold_.follow(-1);
    if (!available()) return;
    if (view==SettingsView::DateTime) {
        std::tm jst{}; TimeUs subsecond=0;
        CivilTime start{TimeService::MinYear,1,1,0,0,0};
        if (time_->now(jst,subsecond))
            start={jst.tm_year+1900,jst.tm_mon+1,jst.tm_mday,jst.tm_hour,jst.tm_min,0};
        model_.fields[0]=start.year; model_.fields[1]=start.month; model_.fields[2]=start.day;
        model_.fields[3]=start.hour; model_.fields[4]=start.minute;
    } else if (view==SettingsView::Brightness) {
        model_.fields[0]=store_->get().brightness;
    } else if (view==SettingsView::ScreenOff) {
        model_.fields[0]=screenOffIndex(store_->get().screenOffSec);
    } else if (view==SettingsView::WatchFace) {
        // A new visit starts at the top: the faces, then back.
        faceList_.setRows(buildSettingsFaceRows(faceRows_,faceCount()));
        faceList_.reset();
    }
}
void SettingsScreen::step(int delta) {
    int& value=model_.fields[model_.cursor];
    switch (model_.view) {
    case SettingsView::DateTime:
        switch (model_.cursor) {
        // The day is not folded into the month here: an impossible date is
        // rejected on save, so passing through 31 while picking a month works.
        case 0: value=wrap(value,TimeService::MinYear,TimeService::MaxYear,delta); break;
        case 1: value=wrap(value,1,12,delta); break;
        case 2: value=wrap(value,1,31,delta); break;
        case 3: value=wrap(value,0,23,delta); break;
        default: value=wrap(value,0,59,delta); break;
        }
        break;
    // Wrapping, not clamping: B only ever steps forward, so every level has to
    // be reachable without a second button.
    case SettingsView::Brightness:
        value=wrap(value,BrightnessMin,BrightnessMax,delta,BrightnessStep); break;
    case SettingsView::ScreenOff:
        value=wrap(value,0,ScreenOffChoiceCount-1,delta); break;
    default: break;
    }
}
const char* SettingsScreen::confirm(ScreenOutcome& out) {
    const int fields=settingsFieldCount(model_.view);
    const int actions=settingsActionCount(model_.view);
    const int action=model_.cursor-fields;
    // An action stays where it is: the view does not close, and there is
    // nothing to save, so it never reaches the notice paths below. Action 0 is
    // the statistics overlay, which only ever turns on (docs/plan.md 5.4): one
    // way on purpose, since the overlay is a measurement aid and having no path
    // back to off means no leftover pixels to erase (docs/plan.md 6.3).
    if (action>=0 && action<actions) {
        if (action==0) out.enableStats=true;
        return nullptr;
    }
    const bool cancel=settingsButtonCount(model_.view)==2 && model_.cursor==fields+actions+1;
    if (model_.view==SettingsView::Info || cancel) { openView(SettingsView::Menu); return nullptr; }
    if (model_.view==SettingsView::DateTime) {
        const CivilTime jst{model_.fields[0],model_.fields[1],model_.fields[2],
                            model_.fields[3],model_.fields[4],0};
        switch (time_->save(jst)) {
        case SaveResult::Saved: openView(SettingsView::Menu); return text::TimeSaved;
        case SaveResult::Invalid: return text::InvalidDate;
        case SaveResult::RtcWriteFailed: return text::SaveFailed;
        default: return text::ClockUnavailable;
        }
    }
    Settings next=store_->get();
    if (model_.view==SettingsView::Brightness) next.brightness=uint8_t(model_.fields[0]);
    else next.screenOffSec=ScreenOffChoices[model_.fields[0]];
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
        if (in.decide && hold_.following()) in.decide=hold_.release(-1);
        else if (in.holdChanged && in.hold!=Hold::B) hold_.end();
        ListDecision decision;
        if (model_.view==SettingsView::Menu) {
            out=handleList(menu_,in,now,decision);
            if (decision.decided) openItem(decision.id,out);
            return out;
        }
        out=handleList(faceList_,in,now,decision);
        if (decision.decided) out.notice=chooseFace(decision.id);
        return out;
    }
    // The editors (docs/task12/plan.md 1.5): A moves the focus over the
    // fields and buttons; B steps a field, repeating while held, and carries
    // out a button. There is no separate editing state to enter.
    const int fields=settingsFieldCount(model_.view);
    // B on a field steps it as it goes down (input/HoldRepeat.h).
    if (e.holdChanged && e.hold==Hold::B && hold_.begin(e.holdSince,model_.cursor,model_.cursor<fields)) {
        step(1); out.changed=true;
    }
    if (e.gesture==Gesture::Tap) {
        const auto hit=hitSettings({{width_,height_},model_.view,model_.cursor},e.x,e.y);
        switch (hit.kind) {
        case SettingsHit::Field:
            model_.cursor=hit.index; out.changed=true; break;
        case SettingsHit::Up:
        case SettingsHit::Down:
            model_.cursor=hit.index;
            step(hit.kind==SettingsHit::Up ? 1 : -1); out.changed=true; break;
        case SettingsHit::Action:
            model_.cursor=fields+hit.index;
            out.changed=true; out.notice=confirm(out); break;
        case SettingsHit::Button:
            model_.cursor=fields+settingsActionCount(model_.view)+hit.index;
            out.changed=true; out.notice=confirm(out); break;
        case SettingsHit::None: break;
        }
        hold_.follow(model_.cursor);
        return out;
    }
    if (e.next) {
        model_.cursor=(model_.cursor+1)%settingsSlotCount(model_.view);
        hold_.follow(model_.cursor);
        out.changed=true;
    }
    if (e.decide) {
        if (hold_.release(model_.cursor)) {
            if (model_.cursor<fields) step(1);
            else out.notice=confirm(out);
        }
        out.changed=true;
    } else if (e.holdChanged && e.hold!=Hold::B) hold_.end();
    return out;
}
bool SettingsScreen::tick(TimeUs now) {
    if (auto* l=shownList()) return l->update(now);
    if (!hold_.take(now)) return false;
    step(1);
    return true;
}
}
