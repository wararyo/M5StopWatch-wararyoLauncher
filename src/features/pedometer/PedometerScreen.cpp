#include "PedometerScreen.h"
#include "features/pedometer/PedometerLayout.h"
namespace launcher {
void PedometerScreen::enter(TimeUs now) {
    shown_=true;
    read(now);
}
bool PedometerScreen::read(TimeUs now) {
    const PedometerModel before=model_;
    if (service_) {
        service_->refresh(now);
        model_.available=service_->available();
        model_.steps=service_->today();
    }
    nextRead_=shown_ ? now+ReadUs : INT64_MAX;
    return model_.available!=before.available || model_.steps!=before.steps;
}
bool PedometerScreen::tick(TimeUs now) {
    return shown_ && read(now);
}
ScreenOutcome PedometerScreen::handle(const Events& e,TimeUs) {
    ScreenOutcome out;
    const bool tapped=e.gesture==Gesture::Tap && pedometerOkHitBox({width_,height_}).contains(e.x,e.y);
    if (e.next || e.decide || tapped) out.leave=true;
    return out;
}
}
