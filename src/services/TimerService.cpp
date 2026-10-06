#include "TimerService.h"
#include <algorithm>
namespace launcher {
int32_t normalizeTimer(int hours,int minutes,int seconds) {
    const int64_t total=int64_t(std::max(hours,0))*3600+int64_t(std::max(minutes,0))*60+std::max(seconds,0);
    return int32_t(std::min<int64_t>(total,TimerMaxSeconds));
}
TimeUs timerAlignmentLeft(TimeUs remaining) {
    constexpr TimeUs Minute=60000000,Hour=60*Minute;
    if (remaining>=2*Hour) return remaining-Hour;
    if (remaining>=20*Minute) return 10*Minute;
    if (remaining>=2*Minute) return Minute;
    return 0;
}
bool TimerService::start(TimeUs now,int32_t seconds) {
    if (state_!=TimerState::Idle || seconds<=0 || seconds>TimerMaxSeconds) return false;
    duration_=seconds;
    end_=now+TimeUs(seconds)*1000000;
    alignLeft_=timerAlignmentLeft(end_-now);
    // An alignment already under way began before this count.
    counting_=false;
    state_=TimerState::Running;
    return true;
}
bool TimerService::pause(TimeUs now) {
    if (state_!=TimerState::Running || now>=end_) return false;
    left_=end_-now;
    state_=TimerState::Paused;
    return true;
}
bool TimerService::resume(TimeUs now) {
    if (state_!=TimerState::Paused) return false;
    end_=now+left_;
    alignLeft_=timerAlignmentLeft(left_);
    state_=TimerState::Running;
    return true;
}
bool TimerService::reset() {
    if (state_!=TimerState::Running && state_!=TimerState::Paused) return false;
    state_=TimerState::Idle;
    counting_=false;
    return true;
}
bool TimerService::expire(TimeUs now) {
    if (state_!=TimerState::Running || now<end_) return false;
    expiredAt_=end_;
    state_=TimerState::Ringing;
    return true;
}
bool TimerService::dismiss() {
    if (state_!=TimerState::Ringing) return false;
    state_=TimerState::Idle;
    return true;
}
void TimerService::alignmentBegun(TimeUs now) {
    counting_=state_==TimerState::Running;
    if (!counting_) return;
    // Begun for the point it was planned at, however late the loop got here,
    // so the next one follows from that point; begun early (the panel came
    // on), from what is left now.
    const TimeUs left=alignLeft_>0 && now>=end_-alignLeft_ ? alignLeft_ : end_-now;
    alignLeft_=timerAlignmentLeft(left);
}
void TimerService::clockCorrected(TimeUs gainUs) {
    if (!counting_) return;
    counting_=false;
    // Ahead means less really went by: more is left, and the end is later.
    if (state_==TimerState::Running) end_+=gainUs;
    else if (state_==TimerState::Paused) left_=std::max<TimeUs>(1,left_+gainUs);
}
TimeUs TimerService::remaining(TimeUs now) const {
    if (state_==TimerState::Running) return std::max<TimeUs>(0,end_-now);
    return state_==TimerState::Paused ? left_ : 0;
}
TimeUs TimerService::overrun(TimeUs now) const {
    return state_==TimerState::Ringing ? std::max<TimeUs>(0,now-expiredAt_) : 0;
}
}
