#pragma once
#include "core/Time.h"
namespace launcher {
// A feature's work that runs beside the screens rather than in one
// (docs/task13/plan.md 2.4): the pedometer ending its day at 04:00 with the
// panel dark, say. The runtime calls it on the UI task; it owns no screen and
// asks for no attention, and what it does is the feature's own.
class HostRoutine {
public:
    virtual ~HostRoutine()=default;
    // Once, when the runtime begins: after the hardware, the clock and the
    // records are set up.
    virtual void begin(TimeUs) {}
    // Every step, with the panel lit or dark. Returns when it is next due
    // (INT64_MAX: not on its own), which is waited for with the panel dark
    // too. `changed` when what a lit panel shows may have moved.
    virtual TimeUs service(TimeUs now,bool& changed)=0;
    // The panel has just gone dark: what should land before a long sleep.
    virtual void panelOff(TimeUs) {}
    // A frame is about to draw the clock, before the applications' labels are
    // collected: the place to read what changes on its own (the IMU's count),
    // at most as often as the feature likes, without waking anything for it.
    virtual void beforeClock(TimeUs) {}
};
}
