#pragma once
#include "features/home/HomeModel.h"
namespace launcher {
class DisplayDataSource {
public:
    virtual ~DisplayDataSource()=default;
    virtual WatchData sample(TimeUs) { return {}; }
    virtual TimeUs nextUpdate(TimeUs) const { return INT64_MAX; }
    // USB power came or went: the battery is read again on the next frame
    // rather than at its period, so charging shows within about a second.
    virtual void refreshBattery() {}
    // The panel came on, or the runtime started: a clock kept through light
    // sleep is put right against its reference.
    virtual void panelWoke(TimeUs) {}
    // Work between frames on the UI task. Returns when it is next due
    // (INT64_MAX: nothing) and sets `changed` when what a frame shows moved.
    virtual TimeUs service(TimeUs, bool& changed) { changed = false; return INT64_MAX; }
};
}
