#include "TimerScreen.h"
#include "features/timer/TimerVibration.h"
#include "i18n/Strings.h"
#include <algorithm>
namespace launcher {
namespace {
constexpr TimeUs FillFrameUs=16000;
TimerView viewOf(TimerState state) {
    switch (state) {
    case TimerState::Idle: return TimerView::Setup;
    case TimerState::Ringing: return TimerView::Ringing;
    default: return TimerView::Countdown;
    }
}
}
void TimerScreen::enter(TimeUs now) {
    shown_=true; fresh_=true;
    hold_=HoldRepeat{}; fillSince_=-1; touchOnReset_=false; spentA_=spentTouch_=false;
    sample(now);
}
void TimerScreen::exit() {
    spentA_=spentTouch_=false;
    // Leaving the alert is dismissing it, home included (docs/task12/plan.md
    // 2.4). Leaving the setup drops its unstarted edit; a countdown runs on.
    // Only a screen that is shown leaves anything: home and presenting call
    // every screen's exit(), and a view left from an earlier visit must not
    // dismiss a timer that rings now.
    if (timer_ && shown_ && model_.view==TimerView::Ringing) timer_->dismiss();
    shown_=false;
    hold_=HoldRepeat{}; fillSince_=-1; touchOnReset_=false;
    ringStart_=-1; vibration_=0;
    nextShown_=nextAlert_=nextFill_=INT64_MAX;
    fresh_=true;
}
void TimerScreen::setupFrom(int32_t seconds) {
    splitTimer(seconds,model_.fields);
    // Minutes first: the field a kitchen timer is most often set by
    // (docs/Images/Timer/Setup.png, agreed 2026-10-06).
    model_.focus=1;
    hold_=HoldRepeat{};
}
void TimerScreen::sample(TimeUs now) {
    if (!available()) return;
    const TimerState state=timer_->state();
    const TimerView view=viewOf(state);
    if (fresh_ || view!=model_.view) {
        fresh_=false;
        model_.view=view;
        fillSince_=-1; touchOnReset_=false;
        // A new setup starts from the length last started; a ringing view
        // starts its alert now.
        if (view==TimerView::Setup) setupFrom(preferences_->value());
        ringStart_=view==TimerView::Ringing ? now : -1;
    }
    nextShown_=nextAlert_=INT64_MAX;
    vibration_=0;
    model_.paused=state==TimerState::Paused;
    if (view==TimerView::Countdown) {
        model_.seconds=timerShownRemaining(timer_->remaining(now));
        // The shown second changes when what is left reaches a whole second.
        if (state==TimerState::Running && model_.seconds>0)
            nextShown_=timer_->deadline()-TimeUs(model_.seconds-1)*TimerSecondUs;
    } else if (view==TimerView::Ringing) {
        model_.seconds=timerShownOverrun(timer_->overrun(now));
        if (model_.seconds<TimerMaxSeconds)
            nextShown_=timer_->expiredAt()+TimeUs(model_.seconds+1)*TimerSecondUs;
        const auto step=timerVibration(now-ringStart_);
        vibration_=step.level;
        nextAlert_=step.until==INT64_MAX ? INT64_MAX : ringStart_+step.until;
    }
    if (fillSince_>=0) {
        const TimeUs held=std::max<TimeUs>(0,now-fillSince_);
        model_.resetFill=uint16_t(std::min<TimeUs>(1000,held*1000/InputController::LongPressUs));
        // A frame for the fill, and one exactly when it is full: the reset.
        nextFill_=model_.resetFill<1000 ? std::min(now+FillFrameUs,fillSince_+InputController::LongPressUs) : INT64_MAX;
    } else {
        model_.resetFill=0;
        nextFill_=INT64_MAX;
    }
}
TimeUs TimerScreen::nextUpdate() const {
    if (model_.view==TimerView::Setup) return hold_.nextUpdate();
    return std::min({nextShown_,nextAlert_,nextFill_});
}
bool TimerScreen::tick(TimeUs now) {
    if (!available()) return false;
    if (model_.view==TimerView::Setup) {
        if (!hold_.take(now)) return false;
        model_.fields[model_.focus]=stepTimerField(model_.focus,model_.fields[model_.focus]);
        return true;
    }
    if (model_.view==TimerView::Countdown && fillSince_>=0 && now-fillSince_>=InputController::LongPressUs)
        resetByHold();
    sample(now);
    return true;
}
bool TimerScreen::clockCorrected(TimeUs now) {
    if (!available() || model_.view==TimerView::Setup) return false;
    sample(now);
    return true;
}
void TimerScreen::resetByHold() {
    // Reset while still held (2026-10-06): the setup comes back the moment
    // the fill completes, not when the wearer lets go.
    spentA_=!touchOnReset_; spentTouch_=touchOnReset_;
    reset();
}
TimeUs TimerScreen::holdPanelUntil() const {
    return model_.view==TimerView::Ringing && ringStart_>=0 ? ringStart_+TimerNoticeUs : 0;
}
bool TimerScreen::start(TimeUs now,ScreenOutcome& out) {
    const int32_t seconds=normalizeTimer(model_.fields[0],model_.fields[1],model_.fields[2]);
    if (seconds<=0 || !timer_->start(now,seconds)) return false;
    // The timer runs whatever storage says; a failed write is only reported
    // (docs/task12/plan.md 2.5). Unbound storage (the render check) says nothing.
    if (preferences_->remember(seconds)!=PrefResult::Ok && preferences_->bound()) out.notice=text::SaveFailed;
    sample(now);
    return true;
}
void TimerScreen::reset() {
    timer_->reset();
    fillSince_=-1; touchOnReset_=false;
}
void TimerScreen::togglePause(TimeUs now) {
    if (timer_->state()==TimerState::Running) timer_->pause(now);
    else timer_->resume(now);
}
bool TimerScreen::handleSetup(const Events& e,TimeUs now,ScreenOutcome& out) {
    int& focus=model_.focus;
    bool changed=false;
    // B on a field steps it as it goes down (input/HoldRepeat.h).
    if (e.holdChanged && e.hold==Hold::B && hold_.begin(e.holdSince,focus,focus<TimerFieldCount)) {
        model_.fields[focus]=stepTimerField(focus,model_.fields[focus]);
        changed=true;
    }
    if (e.gesture==Gesture::Tap) {
        const auto hit=hitTimer({width_,height_},TimerView::Setup,e.x,e.y);
        switch (hit.kind) {
        case TimerHit::Field: focus=hit.index; changed=true; break;
        case TimerHit::Key:
            // With SET focused there is no field to type into.
            if (focus<TimerFieldCount) { model_.fields[focus]=typeTimerField(model_.fields[focus],hit.index); changed=true; }
            break;
        case TimerHit::Set: changed=start(now,out); break;
        default: break;
        }
        hold_.follow(focus);
        // B let go in this same sample: the tap decided it, but the hold ends
        // here all the same, or its repeats would run on with nothing held.
        if (e.decide || (e.holdChanged && e.hold!=Hold::B)) hold_.end();
        return changed;
    }
    if (e.next) {
        focus=(focus+1)%TimerFocusCount;
        hold_.follow(focus);
        changed=true;
    }
    if (e.decide) {
        if (hold_.release(focus)) {
            if (focus<TimerFieldCount) model_.fields[focus]=stepTimerField(focus,model_.fields[focus]);
            else start(now,out);
        }
        changed=true;
    } else if (e.holdChanged && e.hold!=Hold::B) hold_.end();
    return changed;
}
bool TimerScreen::handleCountdown(const Events& e,TimeUs now) {
    const Viewport m{width_,height_};
    bool changed=false;
    // A held alone fills RESET; anything else that ends the hold empties it.
    if (e.holdChanged) {
        if (e.hold==Hold::A) fillSince_=e.holdSince;
        else if (!touchOnReset_) fillSince_=-1;
        changed=true;
    }
    // Released sooner than 600ms: nothing, a reset takes a decision. Held
    // that long it already reset (tick); a release that still finds the
    // countdown, because no frame came in time, resets now.
    if (e.next) {
        fillSince_=-1;
        if (e.pressUs>=InputController::LongPressUs) reset();
        changed=true;
    }
    if (e.decide) { togglePause(now); changed=true; }
    switch (e.gesture) {
    case Gesture::TouchStart:
        if (hitTimer(m,TimerView::Countdown,e.x,e.y).kind==TimerHit::Reset) {
            touchOnReset_=true; fillSince_=now; changed=true;
        }
        break;
    case Gesture::Tap: {
        const auto hit=hitTimer(m,TimerView::Countdown,e.x,e.y).kind;
        // The same for a finger; held 600ms it already reset (tick).
        if (touchOnReset_) {
            touchOnReset_=false; fillSince_=-1;
            if (hit==TimerHit::Reset && e.touchUs>=InputController::LongPressUs) reset();
        } else if (hit==TimerHit::Pause) togglePause(now);
        changed=true;
        break;
    }
    case Gesture::DragStart: case Gesture::DragEnd: case Gesture::Cancel:
        if (touchOnReset_) { touchOnReset_=false; fillSince_=-1; changed=true; }
        break;
    default: break;
    }
    return changed;
}
ScreenOutcome TimerScreen::handle(const Events& e,TimeUs now) {
    ScreenOutcome out{};
    if (!available()) { out.leave=true; return out; }
    // The rest of a press that reset by holding goes nowhere: A's release,
    // and everything of the touch until the finger lifts.
    Events in=e;
    if (spentA_ && (in.next || (in.holdChanged && in.hold!=Hold::A))) { spentA_=false; in.next=false; }
    if (spentTouch_ && in.gesture!=Gesture::None) {
        if (in.gesture==Gesture::TouchStart) spentTouch_=false;
        else {
            if (in.gesture==Gesture::Tap || in.gesture==Gesture::DragEnd || in.gesture==Gesture::Cancel) spentTouch_=false;
            in.gesture=Gesture::None;
        }
    }
    sample(now);
    bool changed=false;
    switch (model_.view) {
    case TimerView::Setup: changed=handleSetup(in,now,out); break;
    case TimerView::Countdown: changed=handleCountdown(in,now); break;
    case TimerView::Ringing: {
        // The alert has one target, so A and B both dismiss it. Only presses
        // that began after it arrive here (InputController::discardHeld).
        const bool tapped=in.gesture==Gesture::Tap &&
            hitTimer({width_,height_},TimerView::Ringing,in.x,in.y).kind==TimerHit::Dismiss;
        if (in.next || in.decide || tapped) { timer_->dismiss(); changed=true; }
        break;
    }
    }
    if (!changed) return out;
    sample(now);
    out.changed=true;
    return out;
}
}
