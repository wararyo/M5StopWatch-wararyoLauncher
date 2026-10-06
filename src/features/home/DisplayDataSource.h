#pragma once
#include "features/home/HomeModel.h"
#include "services/ClockFollower.h"
namespace launcher {
class DisplayDataSource {
public:
    virtual ~DisplayDataSource()=default;
    virtual WatchData sample(TimeUs) { return {}; }
    virtual TimeUs nextUpdate(TimeUs) const { return INT64_MAX; }
    // USB power came or went: the battery is read again on the next frame
    // rather than at its period, so charging shows within about a second.
    virtual void refreshBattery() {}
    // A clock kept through light sleep is put right against its reference:
    // when the runtime starts, when the panel comes on, and when a service
    // that counts through the dark asks (services/ClockFollower.h).
    virtual void alignClock(TimeUs) {}
    // Work between frames on the UI task. Returns when it is next due
    // (INT64_MAX: nothing) and says what aligning the clock did: whether what a
    // frame shows moved, and what the monotonic clock was found to have gained.
    virtual TimeUs service(TimeUs, ClockAlignment& alignment) { alignment = {}; return INT64_MAX; }
};
}
