#include "TimerService.h"
#include <algorithm>
namespace launcher {
int32_t normalizeTimer(int hours,int minutes,int seconds) {
    const int64_t total=int64_t(std::max(hours,0))*3600+int64_t(std::max(minutes,0))*60+std::max(seconds,0);
    return int32_t(std::min<int64_t>(total,TimerMaxSeconds));
}
bool TimerService::start(TimeUs now,int32_t seconds) {
    if (state_!=TimerState::Idle || seconds<=0 || seconds>TimerMaxSeconds) return false;
    duration_=seconds;
    end_=now+TimeUs(seconds)*1000000;
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
    state_=TimerState::Running;
    return true;
}
bool TimerService::reset() {
    if (state_!=TimerState::Running && state_!=TimerState::Paused) return false;
    state_=TimerState::Idle;
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
TimeUs TimerService::remaining(TimeUs now) const {
    if (state_==TimerState::Running) return std::max<TimeUs>(0,end_-now);
    return state_==TimerState::Paused ? left_ : 0;
}
TimeUs TimerService::overrun(TimeUs now) const {
    return state_==TimerState::Ringing ? std::max<TimeUs>(0,now-expiredAt_) : 0;
}
}
