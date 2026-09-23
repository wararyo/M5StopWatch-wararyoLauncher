#pragma once
#include "core/Time.h"
#include <cstdint>
namespace launcher {
enum class StopwatchState : uint8_t { Reset, Running, Paused };
// Lap rows the screen shows. Nothing scrolls, so an older lap can never come
// back into view and the service keeps exactly this many (plan.md 5.3).
inline constexpr int StopwatchLapRows=3;
}
