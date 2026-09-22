#include "StopwatchScreen.h"
namespace launcher {
ScreenModel StopwatchScreen::layoutModel() const {
    ScreenModel m; m.width=width_; m.height=height_; m.screen=ScreenId::Stopwatch;
    m.stopwatch=model_;
    return m;
}
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
    sample(now);
    return true;
}
ScreenOutcome StopwatchScreen::handle(const Events& e,TimeUs now) {
    ScreenOutcome out{};
    if (!available()) { out.leave=true; return out; }
    bool acted=false;
    if (e.gesture==Gesture::Tap) {
        const auto hit=hitStopwatch(layoutModel(),e.x,e.y);
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
