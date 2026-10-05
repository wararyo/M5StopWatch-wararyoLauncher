#pragma once
#include "core/Time.h"
#include <cstdint>
namespace launcher {
// The alert of a timer that ran out (docs/task12/plan.md 1.3, 2.4): weak and
// sparse at first, then stronger and denser, for one minute counted from when
// the alert began, which can be later than the expiry.
inline constexpr TimeUs TimerNoticeUs=60000000;
// Motor levels for the HAL (0..255). Starting points, tuned on the device.
inline constexpr uint8_t VibrationWeak=140,VibrationMedium=190,VibrationStrong=255;
// What the motor does `sinceNotice` into the alert, and when that changes
// next, as an offset from the same start. INT64_MAX once the alert is over.
//
// Every pulse is well under a second: the motor's PWM is on M5IOE1, which
// sleeps after a second of quiet I2C, so each pulse is ended by a write that
// still finds it awake.
struct VibrationStep { uint8_t level=0; TimeUs until=INT64_MAX; };
inline VibrationStep timerVibration(TimeUs sinceNotice) {
    struct Pulse { TimeUs on,off; };
    struct Phase { TimeUs begin,period; uint8_t level; Pulse pulses[2]; int count; };
    static constexpr Phase Phases[]{
        {0,2000000,VibrationWeak,{{0,200000},{0,0}},1},
        {10000000,2000000,VibrationMedium,{{0,200000},{500000,700000}},2},
        {20000000,600000,VibrationStrong,{{0,400000},{0,0}},1},
    };
    const TimeUs t=sinceNotice<0 ? 0 : sinceNotice;
    if (t>=TimerNoticeUs) return {};
    int index=0;
    while (index+1<int(sizeof(Phases)/sizeof(Phases[0])) && t>=Phases[index+1].begin) ++index;
    const Phase& phase=Phases[index];
    const TimeUs end=index+1<int(sizeof(Phases)/sizeof(Phases[0])) ? Phases[index+1].begin : TimerNoticeUs;
    const TimeUs local=(t-phase.begin)%phase.period;
    const TimeUs cycle=t-local;
    VibrationStep step{0,cycle+phase.period};
    for (int i=0;i<phase.count;++i) {
        const Pulse& p=phase.pulses[i];
        if (local>=p.on && local<p.off) { step={phase.level,cycle+p.off}; break; }
        if (local<p.on) { step.until=cycle+p.on; break; }
    }
    if (step.until>end) step.until=end;
    return step;
}
}
