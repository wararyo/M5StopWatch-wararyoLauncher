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
// What `bootSlot` has to stop once the boot is certain. It runs on the UI task,
// after the boot partition has been set and before the restart, and it is not
// called at all when the API fails: that is exactly the split plan.md 8.2 asks
// for, where a failed launch leaves the stopwatch running and a successful one
// ends it.
class BootShutdown {
public:
    virtual ~BootShutdown()=default;
    virtual void onBootCommitted()=0;
};
class SlotService {
public:
    virtual ~SlotService()=default;
    virtual void requestScan()=0;
    // True when the catalog changed since the last poll. UI task only.
    virtual bool poll(SlotCatalog& out)=0;
    virtual bool boot(int slot,const char** message)=0;
    void bindShutdown(BootShutdown* hook) { shutdown_=hook; }
protected:
    BootShutdown* shutdown_=nullptr;
};
}
