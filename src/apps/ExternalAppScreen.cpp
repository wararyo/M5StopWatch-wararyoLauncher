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
    // The cursor is always on the single button, so A has nowhere to move it
    // and the button is drawn selected from the moment the screen opens.
    return m;
}
ScreenModel ExternalAppScreen::layoutModel() const {
    ScreenModel m; m.width=width_; m.height=height_; m.screen=ScreenId::External;
    m.external=model();
    return m;
}
void ExternalAppScreen::enter(TimeUs) {
    issued_=false; message_=nullptr;
    // A launchable slot was already decided in the list, so there is nothing
    // left to confirm and this is the final decision (plan.md 8.2). Everything
    // that can fail has already been saved by the screen that owned it.
    // A slot that becomes Ready later, while this screen is open, is NOT
    // launched: only the decision in the list starts one.
    phase_=model().status==SlotStatus::Ready ?
        ExternalPhase::BootCommitting : ExternalPhase::Browsing;
}
void ExternalAppScreen::exit() {
    phase_=ExternalPhase::Browsing; issued_=false; message_=nullptr;
}
ScreenOutcome ExternalAppScreen::handle(const Events& e,TimeUs) {
    ScreenOutcome out{};
    if (!available()) { out.leave=true; return out; }
    // The commit is not cancellable and must not be disturbed, so the screen
    // drops every event until the API has answered (plan.md 8.2 step 3).
    if (phase_==ExternalPhase::BootCommitting) return out;
    if (e.gesture==Gesture::Tap) {
        if (hitExternal(layoutModel(),e.x,e.y).kind==ExternalHit::Button) out.leave=true;
        return out;
    }
    // One button, whatever the state: B leaves, and A has nowhere to move to.
    if (e.decide) out.leave=true;
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
    return true;
}
}
