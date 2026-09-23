#include "StopwatchScreen.h"
namespace launcher {
void StopwatchScreen::sample(TimeUs now) {
    if (!stopwatch_) return;
    model_.state=stopwatch_->state();
    model_.elapsedUs=stopwatch_->elapsed(now);
    model_.rows=stopwatch_->lapRows();
    for (int i=0;i<StopwatchLapRows;++i) {
        model_.lapUs[i]=stopwatch_->lapTime(i);
        model_.lapNumber[i]=stopwatch_->lapNumber(i);
    }
    // Only a running measurement needs frames. Paused and Reset are static, so
    // the runtime goes back to blocking on input alone.
    next_=model_.state==StopwatchState::Running ? now+StopwatchFrameUs : INT64_MAX;
}
bool StopwatchScreen::pressLeft(TimeUs now) {
    switch (model_.state) {
    case StopwatchState::Running: stopwatch_->lap(now); return true;
    case StopwatchState::Paused: stopwatch_->reset(); return true;
    case StopwatchState::Reset: break; // Nothing to lap and nothing to clear.
    }
    return false;
}
bool StopwatchScreen::pressRight(TimeUs now) {
    if (model_.state==StopwatchState::Running) stopwatch_->stop(now);
    else stopwatch_->start(now);
    return true;
}
bool StopwatchScreen::tick(TimeUs now) {
    if (!available()) return false;
    const TimeUs due=next_;
    sample(now);
    // Keep the 40Hz cadence on the deadlines themselves. The runtime sleeps
    // right up to them (work 8-4) and wakes up to a tick late, which counted
    // from `now` would stretch every period. A late frame is never replayed:
    // once a whole period is lost, the next one is timed from now.
    if (next_!=INT64_MAX && due!=INT64_MAX && due+StopwatchFrameUs>now) next_=due+StopwatchFrameUs;
    return true;
}
ScreenOutcome StopwatchScreen::handle(const Events& e,TimeUs now) {
    ScreenOutcome out{};
    if (!available()) { out.leave=true; return out; }
    bool acted=false;
    if (e.gesture==Gesture::Tap) {
        const auto hit=hitStopwatch({width_,height_},e.x,e.y);
        if (hit.kind==StopwatchHit::Left) acted=pressLeft(now);
        else if (hit.kind==StopwatchHit::Right) acted=pressRight(now);
    } else if (e.next) acted=pressLeft(now);
    else if (e.decide) acted=pressRight(now);
    if (!acted) return out;
    sample(now);
    out.changed=true;
    return out;
}
}
