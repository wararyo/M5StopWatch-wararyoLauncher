#pragma once
#include "host/HostRuntime.h"
#include "host/EffectiveSettings.h"
#include "host/ScreenManager.h"
#include "hal/Hal.h"
#include "multifirm/SlotService.h"
#include "services/StopwatchService.h"
namespace launcher {
// What the application ends once an external boot is certain (plan.md 8.2
// step 5). The slot service calls it on the UI task, after the boot partition
// is set and before the restart, and never when the launch fails; leaving a
// screen does not reach it at all. The stop time is read from the monotonic
// clock at that moment, not taken from the last time the UI happened to see.
class HostShutdown final : public BootShutdown {
public:
    HostShutdown(StopwatchService& stopwatch,Hal& clock):stopwatch_(stopwatch),clock_(clock) {}
    void onBootCommitted() override { stopwatch_.stop(clock_.now()); }
private:
    StopwatchService& stopwatch_;
    Hal& clock_;
};
// The application's composition (docs/task9/plan-9-4.md 1): everything that
// lives as long as the launcher, created once here and lent by reference. The
// hardware, the display, storage and the slot service are made by main and
// bound in, so this builds and runs on the PC as well.
//
// Members are built in declaration order and destroyed in reverse: the
// runtime, which drives the screens, goes first, and the services every screen
// borrows go last. Static in main, so none of it sits on the 8KiB UI stack.
class HostApplication {
public:
    HostApplication(Hal& hal,RenderPort& renderer,DisplayDataSource& data,int width,int height)
        :shutdown_(stopwatch_,hal),screens_(stopwatch_,runtimeSettings_,width,height),
         runtime_(hal,renderer,data,screens_) {}
    // The slot service must not keep calling into a shutdown that is gone.
    ~HostApplication() { if (slots_) slots_->bindShutdown(nullptr); }
    HostApplication(const HostApplication&)=delete;
    HostApplication& operator=(const HostApplication&)=delete;
    void bindSettings(SettingsStore& store,TimeService& time) { screens_.bind(&store,&time); }
    // Registers the application's shutdown with the slot service, since the
    // measurement it ends is owned here.
    void bindSlots(SlotService& slots) {
        slots_=&slots; slots.bindShutdown(&shutdown_); runtime_.bindSlots(slots);
    }
    void setInfo(const char* name,const char* version,const char* idf) { screens_.setInfo(name,version,idf); }
    HostRuntime& runtime() { return runtime_; }
    ScreenManager& screens() { return screens_; }
    const StopwatchService& stopwatch() const { return stopwatch_; }
    const RuntimeSettings& runtimeSettings() const { return runtimeSettings_; }
private:
    StopwatchService stopwatch_;
    RuntimeSettings runtimeSettings_{};
    HostShutdown shutdown_;
    ScreenManager screens_;
    HostRuntime runtime_;
    SlotService* slots_=nullptr;
};
}
