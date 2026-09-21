#include "ExternalAppScreen.h"
namespace launcher {
ExternalModel ExternalAppScreen::model() const {
    ExternalModel m{};
    m.slot=slot_; m.phase=phase_; m.message=message_;
    if (catalog_ && slot_>=1 && slot_<=SlotCount) {
        const auto& entry=catalog_->slots[slot_-1];
        m.status=catalog_->layoutSupported ? entry.status : SlotStatus::Unsupported;
        m.name=entry.name[0] ? entry.name : nullptr;
        m.version=entry.version[0] ? entry.version : nullptr;
        m.error=entry.error;
    } else m.status=SlotStatus::Unsupported;
    // A scan finishing while the detail is open changes the button count, so
    // the cursor is clamped here rather than trusted from the last input.
    const int buttons=externalButtonCount(m);
    m.cursor=buttons>0 ? (cursor_<buttons ? cursor_ : buttons-1) : 0;
    return m;
}
ScreenModel ExternalAppScreen::layoutModel() const {
    ScreenModel m; m.width=width_; m.height=height_; m.screen=ScreenId::External;
    m.external=model();
    return m;
}
void ExternalAppScreen::enter() {
    phase_=ExternalPhase::Browsing; cursor_=0; issued_=false; message_=nullptr;
}
void ExternalAppScreen::exit() { enter(); }
void ExternalAppScreen::activate(ScreenOutcome& out) {
    const auto m=model();
    // Ready is the only state with two buttons, and index 0 is the launch. Any
    // other button on any other state simply leaves.
    if (m.status==SlotStatus::Ready && m.phase==ExternalPhase::Browsing && m.cursor==0) {
        // plan.md 8.2 step 2: everything that can fail has already been saved
        // by the screen that owned it. Nothing is left to flush here.
        phase_=ExternalPhase::BootCommitting; issued_=false; message_=nullptr;
        cursor_=0; out.changed=true;
        return;
    }
    out.leave=true;
}
ScreenOutcome ExternalAppScreen::handle(const Events& e,TimeUs) {
    ScreenOutcome out{};
    if (!available()) { out.leave=true; return out; }
    // The commit is not cancellable and must not be disturbed, so the screen
    // drops every event until the API has answered (plan.md 8.2 step 3).
    if (phase_==ExternalPhase::BootCommitting) return out;
    const auto m=model();
    const int buttons=externalButtonCount(m);
    if (buttons<=0) return out;
    if (e.gesture==Gesture::Tap) {
        const auto hit=hitExternal(layoutModel(),e.x,e.y);
        if (hit.kind==ExternalHit::Button) {
            cursor_=hit.index; out.changed=true; activate(out);
        }
        return out;
    }
    if (e.next) { cursor_=(m.cursor+1)%buttons; out.changed=true; }
    if (e.decide) { cursor_=m.cursor; out.changed=true; activate(out); }
    return out;
}
bool ExternalAppScreen::commitPendingBoot() {
    if (phase_!=ExternalPhase::BootCommitting || issued_ || !slots_) return false;
    issued_=true;
    const char* message=nullptr;
    // Returns only on failure: a successful boot restarts inside this call.
    if (slots_->boot(slot_,&message)) return false;
    phase_=ExternalPhase::BootFailed;
    message_=message;
    cursor_=0;
    return true;
}
}
