#pragma once
#include "host/HostRuntime.h"
#include "host/EffectiveSettings.h"
#include "host/ScreenManager.h"
#include "hal/Hal.h"
#include "multifirm/SlotService.h"
#include "services/StopwatchService.h"
#include "services/TimerService.h"
#include "services/PedometerService.h"
#include "storage/TimerPreferences.h"
#include "storage/PedometerRecord.h"
#include "features/background/BackgroundInfoHub.h"
#include "features/stopwatch/StopwatchBackgroundInfo.h"
#include "features/timer/TimerAttention.h"
#include "features/timer/TimerBackgroundInfo.h"
#include "features/pedometer/PedometerRoutine.h"
namespace launcher {
// What the application ends once an external boot is certain (plan.md 8.2
// step 5). The slot service calls it on the UI task, after the boot partition
// is set and before the restart, and never when the launch fails; leaving a
// screen does not reach it at all. The stop time is read from the monotonic
// clock at that moment, not taken from the last time the UI happened to see.
// The timer is stopped outright, ringing or not, and so is the motor: the
// guest knows nothing of either (docs/task12/plan.md 1). Today's steps are
// saved, since the restart begins the IMU's count again (docs/task13/plan.md
// 1.2).
class HostShutdown final : public BootShutdown {
public:
    HostShutdown(StopwatchService& stopwatch,TimerService& timer,PedometerRoutine& pedometer,Hal& hal)
        :stopwatch_(stopwatch),timer_(timer),pedometer_(pedometer),hal_(hal) {}
    void onBootCommitted() override {
        stopwatch_.stop(hal_.now());
        timer_.cancel();
        hal_.setVibration(0);
        pedometer_.save(hal_.now());
    }
private:
    StopwatchService& stopwatch_;
    TimerService& timer_;
    PedometerRoutine& pedometer_;
    Hal& hal_;
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
        :pedometer_(hal),pedometerRoutine_(pedometer_,pedometerRecord_),
         stopwatchInfo_(stopwatch_),timerInfo_(timer_),timerAttention_(timer_),
         shutdown_(stopwatch_,timer_,pedometerRoutine_,hal),
         screens_(stopwatch_,runtimeSettings_,width,height),
         runtime_(hal,renderer,data,screens_,&background_) {
        // Registration order is display order (docs/task10/plan.md 4.1).
        background_.add(stopwatchInfo_);
        background_.add(timerInfo_);
        screens_.bindTimer(&timer_,&timerPreferences_);
        runtime_.bindAttention(timerAttention_);
        runtime_.bindClockFollower(stopwatch_);
        runtime_.bindClockFollower(timer_);
        runtime_.bindClockFollower(pedometer_);
        runtime_.bindRoutine(pedometerRoutine_);
    }
    // The slot service must not keep calling into a shutdown that is gone.
    ~HostApplication() { if (slots_) slots_->bindShutdown(nullptr); }
    HostApplication(const HostApplication&)=delete;
    HostApplication& operator=(const HostApplication&)=delete;
    void bindSettings(SettingsStore& store,TimeService& time) { screens_.bind(&store,&time); pedometer_.bindTime(&time); }
    // The clock layer's input (the renderer, which owns the faces).
    void bindHome(HomeControlPort& home) { screens_.bindHome(&home); }
    // Registers the application's shutdown with the slot service, since the
    // measurement it ends is owned here.
    void bindSlots(SlotService& slots) {
        slots_=&slots; slots.bindShutdown(&shutdown_); runtime_.bindSlots(slots);
    }
    void setInfo(const char* name,const char* version,const char* idf) { screens_.setInfo(name,version,idf); }
    // The records beside the settings: the timer's last length and the
    // pedometer's day are read now. Unbound, the timer starts from its default
    // and remembers in RAM only, and the pedometer starts each run from 0.
    void bindRecords(RecordBackend& records) {
        timerPreferences_.bind(&records); timerPreferences_.load();
        pedometerRecord_.bind(&records); pedometerRecord_.load();
    }
    HostRuntime& runtime() { return runtime_; }
    ScreenManager& screens() { return screens_; }
    const StopwatchService& stopwatch() const { return stopwatch_; }
    TimerService& timer() { return timer_; }
    TimerPreferences& timerPreferences() { return timerPreferences_; }
    PedometerService& pedometer() { return pedometer_; }
    PedometerRoutine& pedometerRoutine() { return pedometerRoutine_; }
    BackgroundInfoHub& background() { return background_; }
    const RuntimeSettings& runtimeSettings() const { return runtimeSettings_; }
private:
    StopwatchService stopwatch_;
    TimerService timer_;
    TimerPreferences timerPreferences_;
    PedometerService pedometer_;
    PedometerRecord pedometerRecord_;
    PedometerRoutine pedometerRoutine_;
    // Service, then the providers that read it, then the hub that collects
    // them, all before the runtime that is lent the hub.
    StopwatchBackgroundInfo stopwatchInfo_;
    TimerBackgroundInfo timerInfo_;
    TimerAttention timerAttention_;
    BackgroundInfoHub background_;
    RuntimeSettings runtimeSettings_{};
    HostShutdown shutdown_;
    ScreenManager screens_;
    HostRuntime runtime_;
    SlotService* slots_=nullptr;
};
}
