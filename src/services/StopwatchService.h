#pragma once
#include "services/Stopwatch.h"
namespace launcher {
// Measurement only: it takes the monotonic time it is given and never reads a
// clock, a display period or storage (plan.md 4). The screen keeps no state of
// its own, so leaving the screen, going home or blanking the panel cannot
// disturb a running measurement.
class StopwatchService {
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
private:
    StopwatchState state_=StopwatchState::Reset;
    TimeUs accumulated_=0,startedAt_=0;
    TimeUs laps_[StopwatchLapRows]{};
    uint16_t numbers_[StopwatchLapRows]{};
    uint16_t lapCount_=0;
    int rows_=0;
};
}
