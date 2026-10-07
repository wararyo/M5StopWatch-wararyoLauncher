#pragma once
#include "services/ClockFollower.h"
#include <cstdint>
namespace launcher {
enum class TimerState : uint8_t { Idle, Running, Paused, Ringing };
// The longest a timer can be set to, 99:59:59 (docs/task12/plan.md 1.1).
inline constexpr int32_t TimerMaxSeconds=99*3600+59*60+59;
// The fields as typed, minutes and seconds up to 99, carried into one total
// and capped at the longest: 01:75:60 is 02:16:00.
int32_t normalizeTimer(int hours,int minutes,int seconds);
// Where a running timer next has the clock aligned with the RTC, as what is
// left then, from what is left now (docs/task12/plan.md 2.1): from two hours
// on, an hour later; from 20 minutes, with ten left; from two minutes, with
// one left. 0 below that: the expiry lights the panel, which aligns it anyway.
TimeUs timerAlignmentLeft(TimeUs remaining);
// The countdown (docs/task12/plan.md 2.1, 2.5). Like the stopwatch it takes
// the monotonic time it is given and reads no clock, so setting the date
// cannot move the end. The monotonic clock gains in light sleep, though, so
// the runtime aligns the clock on the timer's schedule, and the end moves by
// what each alignment finds (services/ClockFollower.h). It knows nothing of
// the screen, the alert or storage: the runtime asks it when it runs out.
class TimerService final : public ClockFollower {
public:
    // From Idle only, for 1s up to the longest. False when nothing started.
    bool start(TimeUs now,int32_t seconds);
    // Running only, and not once it has run out: that is expire()'s.
    bool pause(TimeUs now);
    bool resume(TimeUs now);
    // Running or paused back to Idle. A ringing timer is dismissed instead.
    bool reset();
    // True once, when a running timer has reached its end by `now`. It rings
    // from its end, not from when it was noticed, so the count up is right
    // however late the loop gets here.
    bool expire(TimeUs now);
    bool dismiss();
    // Whatever it was doing, for an external boot (docs/task12/plan.md 1).
    void cancel() { state_=TimerState::Idle; counting_=false; }
    TimerState state() const { return state_; }
    // The length it was started with, in seconds.
    int32_t duration() const { return duration_; }
    // Left to run while running or paused, otherwise 0.
    TimeUs remaining(TimeUs now) const;
    // Since it ran out, while ringing, otherwise 0.
    TimeUs overrun(TimeUs now) const;
    TimeUs expiredAt() const { return expiredAt_; }
    // When it runs out, while running; INT64_MAX otherwise. The runtime waits
    // for it even with the panel dark.
    TimeUs deadline() const { return state_==TimerState::Running ? end_ : INT64_MAX; }
    // Only while running; a paused timer waits for the panel like the rest.
    TimeUs alignmentDue() const override {
        return state_==TimerState::Running && alignLeft_>0 ? end_-alignLeft_ : INT64_MAX;
    }
    void alignmentBegun(TimeUs now) override;
    // Running, the end moves; paused since the alignment began, what is left.
    void clockCorrected(TimeUs gainUs) override;
private:
    TimerState state_=TimerState::Idle;
    int32_t duration_=0;
    TimeUs end_=0,left_=0,expiredAt_=0;
    // What is left at the next alignment, 0: none. Counted back from the end,
    // so it moves with it.
    TimeUs alignLeft_=0;
    // Running when the alignment under way began: its correction is ours.
    bool counting_=false;
};
}
