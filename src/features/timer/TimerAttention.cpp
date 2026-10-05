#include "TimerAttention.h"
namespace launcher {
AttentionRequest TimerAttention::attention(TimeUs now) {
    timer_.expire(now);
    if (timer_.state()!=TimerState::Ringing) { start_=-1; next_=INT64_MAX; return {}; }
    AttentionRequest request;
    request.active=true;
    if (start_<0) return request; // Not started yet: nothing to hold or play.
    request.holdUntil=start_+TimerNoticeUs;
    const auto step=timerVibration(now-start_);
    request.vibration=step.level;
    next_=step.until==INT64_MAX ? INT64_MAX : start_+step.until;
    return request;
}
TimeUs TimerAttention::nextAttention() const {
    // Running: its end. Ringing and started: the pattern's next change; once
    // the minute is over, nothing until the timer is dismissed. Ringing but
    // not yet started (a boot commit holds it): the host asks every step.
    if (timer_.state()==TimerState::Running) return timer_.deadline();
    return timer_.state()==TimerState::Ringing && start_>=0 ? next_ : INT64_MAX;
}
}
