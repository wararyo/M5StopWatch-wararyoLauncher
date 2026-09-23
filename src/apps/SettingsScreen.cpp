#include "SettingsScreen.h"
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
    model_.editing=false;
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
    // Wrapping, not clamping: A only ever steps forward, so every level has to
    // be reachable without a second button.
    case SettingsView::Brightness:
        value=wrap(value,BrightnessMin,BrightnessMax,delta,BrightnessStep); break;
    case SettingsView::ScreenOff:
        value=wrap(value,0,ScreenOffChoiceCount-1,delta); break;
    default: break;
    }
}
const char* SettingsScreen::confirm() {
    const int fields=settingsFieldCount(model_.view);
    const int actions=settingsActionCount(model_.view);
    const int action=model_.cursor-fields;
    // An action stays where it is: the view does not close, and there is
    // nothing to save, so it never reaches the notice paths below. Action 0 is
    // the statistics overlay, which only ever turns on (docs/plan.md 5.4).
    if (action>=0 && action<actions) {
        if (action==0 && runtime_) runtime_->stats=true;
        return nullptr;
    }
    const bool cancel=settingsButtonCount(model_.view)==2 && model_.cursor==fields+actions+1;
    if (model_.view==SettingsView::Info || cancel) { openView(SettingsView::Menu); return nullptr; }
    if (model_.view==SettingsView::DateTime) {
        const CivilTime jst{model_.fields[0],model_.fields[1],model_.fields[2],
                            model_.fields[3],model_.fields[4],0};
        switch (time_->save(jst)) {
        case SaveResult::Saved: openView(SettingsView::Menu); return "時刻を保存しました";
        case SaveResult::Invalid: return "日付が正しくありません";
        case SaveResult::RtcWriteFailed: return "保存に失敗しました";
        default: return "時計を設定できません";
        }
    }
    Settings next=store_->get();
    if (model_.view==SettingsView::Brightness) next.brightness=uint8_t(model_.fields[0]);
    else next.screenOffSec=ScreenOffChoices[model_.fields[0]];
    if (!store_->save(next)) return "保存に失敗しました";
    openView(SettingsView::Menu);
    return "保存しました";
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
// The same order as the launcher's list (ScreenManager::handle), without its
// pull back to the clock: at the top a downward drag only scrolls back.
ScreenOutcome SettingsScreen::handleMenu(const Events& e,TimeUs now) {
    ScreenOutcome out{};
    if (e.gesture==Gesture::TouchStart) { out.changed=menu_.touchStart(); return out; }
    if ((e.gesture==Gesture::Tap || e.gesture==Gesture::DragEnd) && menu_.releaseAfterStop(now)) {
        out.changed=true; return out;
    }
    if (e.gesture==Gesture::Cancel) { menu_.cancel(now); out.changed=true; return out; }
    if (e.gesture==Gesture::DragStart) {
        if (std::abs(e.totalX)>std::abs(e.totalY)) return out;
        menu_.dragStart();
    }
    if (e.gesture==Gesture::DragStart || e.gesture==Gesture::DragMove) {
        out.changed=menu_.dragMove(float(e.totalY)); return out;
    }
    if (e.gesture==Gesture::DragEnd) {
        if (menu_.state().dragging) { menu_.dragEnd(-e.velocityY,now); out.changed=true; }
        return out;
    }
    if (e.next) { out.changed=menu_.next(now); return out; }
    ListDecision decision;
    if (e.gesture==Gesture::Tap)
        decision=menu_.tap(settingsMenuPlacement({width_,height_},menu_.scroll()),e.x,e.y,now);
    else if (e.decide) decision=menu_.decide(now);
    if (!decision.changed) return out;
    out.changed=true;
    if (decision.decided) openItem(decision.id,out);
    return out;
}
ScreenOutcome SettingsScreen::handle(const Events& e,TimeUs now) {
    ScreenOutcome out{};
    if (!available()) return out;
    if (model_.view==SettingsView::Menu) return handleMenu(e,now);
    const int fields=settingsFieldCount(model_.view);
    if (e.gesture==Gesture::Tap) {
        const auto hit=hitSettings({{width_,height_},model_.view,model_.cursor},e.x,e.y);
        switch (hit.kind) {
        case SettingsHit::Field:
            model_.cursor=hit.index; model_.editing=false; out.changed=true; break;
        case SettingsHit::Up:
        case SettingsHit::Down:
            model_.cursor=hit.index; model_.editing=false;
            step(hit.kind==SettingsHit::Up ? 1 : -1); out.changed=true; break;
        case SettingsHit::Action:
            model_.cursor=fields+hit.index; model_.editing=false;
            out.changed=true; out.notice=confirm(); break;
        case SettingsHit::Button:
            model_.cursor=fields+settingsActionCount(model_.view)+hit.index; model_.editing=false;
            out.changed=true; out.notice=confirm(); break;
        case SettingsHit::None: break;
        }
        return out;
    }
    if (e.next) {
        if (model_.editing) step(1);
        else model_.cursor=(model_.cursor+1)%settingsSlotCount(model_.view);
        out.changed=true;
    }
    if (e.decide) {
        // B enters a field, then confirms it. On a button it acts straight away.
        if (model_.cursor<fields) model_.editing=!model_.editing;
        else { model_.editing=false; out.notice=confirm(); }
        out.changed=true;
    }
    return out;
}
}
