#include "StopwatchService.h"
#include <cstdio>
namespace launcher {
void formatStopwatch(TimeUs elapsed,char* clock,size_t clockSize,
                     char* fraction,size_t fractionSize) {
    // A negative value cannot come from a monotonic source, but clamping costs
    // nothing and keeps the field widths honest.
    if (elapsed<0) elapsed=0;
    if (elapsed>StopwatchDisplayCapUs) elapsed=StopwatchDisplayCapUs;
    const int64_t hundredths=elapsed/10000;
    std::snprintf(clock,clockSize,"%02d:%02d:%02d",int(hundredths/360000),
                  int((hundredths/6000)%60),int((hundredths/100)%60));
    std::snprintf(fraction,fractionSize,".%02d",int(hundredths%100));
}
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
