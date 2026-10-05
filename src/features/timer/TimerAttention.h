#pragma once
#include "host/AttentionSource.h"
#include "services/TimerService.h"
#include "features/timer/TimerVibration.h"
namespace launcher {
// The timer's attention request (docs/task12/plan.md 1.3, 2.4): the service
// only counts, and this turns a timer that ran out into a request. It asks
// for as long as the timer rings; for the first minute from when the host
// started it, the panel is held lit and the motor follows the pattern.
// Dismissing the timer ends the request, and with it both.
class TimerAttention final : public AttentionSource {
public:
    explicit TimerAttention(TimerService& timer):timer_(timer) {}
    AttentionRequest attention(TimeUs now) override;
    void attended(TimeUs now) override { start_=now; }
    TimeUs nextAttention() const override;
    // Within the alert's minute: the motor's pattern still runs.
    bool alerting(TimeUs now) const { return start_>=0 && now-start_<TimerNoticeUs; }
private:
    TimerService& timer_;
    TimeUs start_=-1;            // When the host started the request, -1 before.
    TimeUs next_=INT64_MAX;      // The pattern's next change, while alerting.
};
}
