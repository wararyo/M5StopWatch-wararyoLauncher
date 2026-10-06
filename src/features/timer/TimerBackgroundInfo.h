#pragma once
#include "features/background/BackgroundInfo.h"
#include "services/TimerService.h"
#include <cstddef>
namespace launcher {
// The timer's line on the watch face (docs/task12/plan.md 1.4, 2.5): only
// while it runs, the time left as `mm:ss` under an hour and `HH:mm` from then
// on, with the timer's own icon and colour. Paused, ringing or idle, nothing:
// a paused timer is not counting, and a ringing one has the screen. Like the
// stopwatch's line it reads the service and nothing else, and its deadlines
// follow the countdown, not the wall clock.
class TimerBackgroundInfo final : public BackgroundInfoProvider {
public:
    explicit TimerBackgroundInfo(const TimerService& service):service_(service) {}
    LaunchTargetId id() const override { return LaunchTargetId::Timer; }
    bool sample(TimeUs now,BackgroundInfo& out) const override;
private:
    const TimerService& service_;
};
// The label for `remaining` and how long until it next changes. Rounded up,
// not down as the stopwatch's is: seconds first, then, from an hour on, those
// seconds up to minutes, so 3601s reads 01:01, 3600s 01:00 and 3599s 59:59.
// Nothing left reads 00:00 and never changes (INT64_MAX).
TimeUs formatTimerBackground(TimeUs remaining,char* label,size_t size);
}
