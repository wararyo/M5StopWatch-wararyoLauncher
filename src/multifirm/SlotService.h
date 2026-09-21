#pragma once
#include "SlotCatalog.h"
namespace launcher {
// The boundary between the UI and MultiFirm, in the same spirit as `Hal`:
// everything above this line builds and runs on the PC.
//
// `boot` is deliberately synchronous. MultiFirm's `bootSlot` runs its shutdown
// callback on the calling task, and what has to be stopped there (the display,
// and from task 5 the stopwatch) belongs to the UI task, so the UI task is the
// only correct caller. It returns only on failure; success ends in a restart.
class SlotService {
public:
    virtual ~SlotService()=default;
    virtual void requestScan()=0;
    // True when the catalog changed since the last poll. UI task only.
    virtual bool poll(SlotCatalog& out)=0;
    virtual bool boot(int slot,const char** message)=0;
};
}
