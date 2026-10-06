#pragma once
#include "core/Time.h"
#include <cstdint>
namespace launcher {
// What one step of aligning the system clock with the RTC did
// (services/TimeService.h).
struct ClockAlignment {
    // The clock was set at the RTC's second edge: what a frame shows moved.
    bool stepped=false;
    // How far the monotonic clock had run ahead of the RTC (negative: behind)
    // since it was last set at an edge. The system clock is the monotonic one
    // plus an offset, so this is what light sleep added to every count. Not
    // measured when the clock was last set off an edge (at boot, by hand),
    // since that setting's own fraction of a second would be in it.
    bool measured=false;
    TimeUs gainUs=0;
};
// A service that counts real time on the monotonic clock, which gains on the
// internal RC oscillator through light sleep (docs/task12/plan.md 2.1). The
// runtime aligns the clock with the RTC when the service asks, and whenever
// the panel comes on, and hands it what the alignment found.
//
// The drift is all from the dark, where nothing the service counts can change
// state: a press lights the panel first, and an expiry lights it too. So what
// was counting when an alignment began is what the correction belongs to.
class ClockFollower {
public:
    virtual ~ClockFollower()=default;
    // When the clock should next be aligned, to be woken for then even with
    // the panel dark. INT64_MAX: not before the panel next comes on.
    virtual TimeUs alignmentDue() const { return INT64_MAX; }
    // An alignment begins now: whatever is counting takes the correction it
    // brings, and the next one is planned from here.
    virtual void alignmentBegun(TimeUs now)=0;
    // The alignment found the monotonic clock `gainUs` ahead of real time.
    virtual void clockCorrected(TimeUs gainUs)=0;
};
}
