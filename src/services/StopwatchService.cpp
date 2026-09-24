#include "StopwatchService.h"
namespace launcher {
void StopwatchService::start(TimeUs now) {
    if (state_==StopwatchState::Running) return;
    startedAt_=now;
    state_=StopwatchState::Running;
}
void StopwatchService::stop(TimeUs now) {
    if (state_!=StopwatchState::Running) return;
    accumulated_+=now-startedAt_;
    state_=StopwatchState::Paused;
}
void StopwatchService::reset() {
    accumulated_=0; startedAt_=0;
    state_=StopwatchState::Reset;
    rows_=0; lapCount_=0;
}
void StopwatchService::lap(TimeUs now) {
    // Only a running measurement has a lap to take; the button is dead in the
    // other two states (plan.md 5.3).
    if (state_!=StopwatchState::Running) return;
    ++lapCount_;
    for (int i=StopwatchLapRows-1;i>0;--i) { laps_[i]=laps_[i-1]; numbers_[i]=numbers_[i-1]; }
    laps_[0]=elapsed(now); numbers_[0]=lapCount_;
    if (rows_<StopwatchLapRows) ++rows_;
}
TimeUs StopwatchService::elapsed(TimeUs now) const {
    return state_==StopwatchState::Running ? accumulated_+(now-startedAt_) : accumulated_;
}
}
