#pragma once
#include "services/Stopwatch.h"
#include "services/ClockFollower.h"
namespace launcher {
// Measurement only: it takes the monotonic time it is given and never reads a
// clock, a display period or storage (plan.md 4). The screen keeps no state of
// its own, so leaving the screen, going home or blanking the panel cannot
// disturb a running measurement. What the monotonic clock gains in the dark is
// taken off when the panel comes on and aligns the clock with the RTC
// (services/ClockFollower.h); a stopwatch has no end to be woken for.
class StopwatchService final : public ClockFollower {
public:
    void start(TimeUs now);
    void stop(TimeUs now);
    void reset();
    void lap(TimeUs now);
    TimeUs elapsed(TimeUs now) const;
    StopwatchState state() const { return state_; }
    // Total laps taken, which keeps counting after older rows fall off.
    uint16_t lapCount() const { return lapCount_; }
    int lapRows() const { return rows_; }
    // Row 0 is the newest lap.
    TimeUs lapTime(int row) const { return row>=0 && row<rows_ ? laps_[row] : 0; }
    uint16_t lapNumber(int row) const { return row>=0 && row<rows_ ? numbers_[row] : 0; }
    void alignmentBegun(TimeUs) override { counting_=state_==StopwatchState::Running; }
    // Running, the start moves later; stopped since the alignment began, the
    // total shrinks. Laps already taken stay as they were read.
    void clockCorrected(TimeUs gainUs) override;
private:
    StopwatchState state_=StopwatchState::Reset;
    TimeUs accumulated_=0,startedAt_=0;
    TimeUs laps_[StopwatchLapRows]{};
    uint16_t numbers_[StopwatchLapRows]{};
    uint16_t lapCount_=0;
    int rows_=0;
    // Running when the alignment under way began: its correction is ours.
    bool counting_=false;
};
}
