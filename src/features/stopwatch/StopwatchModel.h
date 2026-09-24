#pragma once
#include "services/Stopwatch.h"
#include <cstddef>
namespace launcher {
inline constexpr TimeUs StopwatchDisplayCapUs=100LL*3600*1000000-10000;
inline constexpr TimeUs StopwatchFrameUs=25000;
void formatStopwatch(TimeUs elapsed,char* clock,size_t clockSize,
                     char* fraction,size_t fractionSize);
struct StopwatchModel {
    StopwatchState state=StopwatchState::Reset;
    TimeUs elapsedUs=0;
    int rows=0;
    TimeUs lapUs[StopwatchLapRows]{};
    uint16_t lapNumber[StopwatchLapRows]{};
};
}
