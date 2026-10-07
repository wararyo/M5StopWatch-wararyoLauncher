#pragma once
#include "core/Time.h"
#include "services/TimerService.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
namespace launcher {
inline constexpr TimeUs TimerSecondUs=1000000;
// What is left, as shown: rounded up, so the length set shows at the start
// and 00:00:00 never shows before the timer rings (docs/task12/plan.md 2.5).
inline int32_t timerShownRemaining(TimeUs left) {
    return left<=0 ? 0 : int32_t((left+TimerSecondUs-1)/TimerSecondUs);
}
// Since it ran out, as shown: rounded down, and held at 99:59:59.
inline int32_t timerShownOverrun(TimeUs since) {
    if (since<=0) return 0;
    const TimeUs seconds=since/TimerSecondUs;
    return seconds>=TimerMaxSeconds ? TimerMaxSeconds : int32_t(seconds);
}
inline void formatTimer(int32_t seconds,char* out,size_t size) {
    if (seconds<0) seconds=0;
    std::snprintf(out,size,"%02d:%02d:%02d",int(seconds/3600),int(seconds/60%60),int(seconds%60));
}
// The three views of the one timer screen, following the service's state:
// setting it up, counting down (running or paused) and ringing.
enum class TimerView : uint8_t { Setup, Countdown, Ringing };
// The setup's focus: the three fields, then SET. The keys take no focus.
inline constexpr int TimerFieldCount=3,TimerFocusSet=3,TimerFocusCount=4;
struct TimerModel {
    TimerView view=TimerView::Setup;
    // Setup: the fields as typed, hours/minutes/seconds, each 00..99.
    int fields[TimerFieldCount]{};
    int focus=1; // Minutes first.
    // Countdown and ringing: the seconds shown.
    int32_t seconds=0;
    bool paused=false;
    // How far RESET is filled while A or a finger holds it, 0..1000.
    uint16_t resetFill=0;
};
}
