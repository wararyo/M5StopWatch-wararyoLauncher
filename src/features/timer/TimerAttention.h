#pragma once
#include "host/AttentionSource.h"
#include "services/TimerService.h"
namespace launcher {
// The timer's attention request (docs/task12/plan.md 1.3, 2.4): the service
// only counts, and this turns a timer that ran out into a request for the
// timer screen, for as long as it rings. The alert itself (the motor, the
// panel held lit) is the screen's (TimerScreen).
class TimerAttention final : public AttentionSource {
public:
    explicit TimerAttention(TimerService& timer):timer_(timer) {}
    AttentionRequest attention(TimeUs now) override {
        timer_.expire(now);
        if (timer_.state()!=TimerState::Ringing) return {};
        return {true,true,ScreenId::Timer};
    }
    // Only a running timer can start a request, at its end.
    TimeUs nextAttention() const override { return timer_.deadline(); }
private:
    TimerService& timer_;
};
}
