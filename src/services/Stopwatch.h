#pragma once
#include "input/InputController.h"
#include <cstddef>
#include <cstdint>
namespace launcher {
enum class StopwatchState : uint8_t { Reset, Running, Paused };
// Lap rows the screen shows. Nothing scrolls, so an older lap can never come
// back into view and the service keeps exactly this many (plan.md 5.3).
inline constexpr int StopwatchLapRows=3;
// 99:59:59.99. The measurement itself keeps running past this; only the display
// stops (plan.md 5.3).
inline constexpr TimeUs StopwatchDisplayCapUs=100LL*3600*1000000-10000;
// 40 Hz while running and visible, which clears the 30 fps of plan.md 6.3 with
// room to spare.
inline constexpr TimeUs StopwatchFrameUs=25000;
// The clock and the hundredths are separate strings because they are separate
// elements: only the hundredths are repainted every frame.
void formatStopwatch(TimeUs elapsed,char* clock,size_t clockSize,
                     char* fraction,size_t fractionSize);
struct StopwatchModel {
    StopwatchState state=StopwatchState::Reset;
    TimeUs elapsedUs=0;
    int rows=0; // Filled lap rows, newest first.
    TimeUs lapUs[StopwatchLapRows]{};
    uint16_t lapNumber[StopwatchLapRows]{};
};
}
