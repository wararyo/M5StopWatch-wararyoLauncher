#pragma once
#include "host/Screen.h"
#include "input/HoldRepeat.h"
#include "services/TimerService.h"
#include "storage/TimerPreferences.h"
#include "features/timer/TimerLayout.h"
namespace launcher {
// The timer's one screen (docs/task12/plan.md 1.1-1.3), whose view follows the
// service: setup while idle, the countdown while running or paused, and the
// alert while ringing. The countdown itself is the service's, so leaving the
// screen never stops it; only the setup's unstarted edit is the screen's.
//
// Buttons follow the general rule (plan.md 5.1): A moves the focus, B steps a
// field (at once, then repeating while held) or carries out SET. On the
// countdown A held for 600ms resets there and then, B pauses and resumes.
// Ringing, A, B or the button dismisses. The keys are touch only.
//
// Ringing, the screen is the alert: for its first minute it holds the panel lit
// and drives the motor (TimerVibration.h), timed from when it was shown, so an
// alert held back by a boot commit still starts at its beginning. Leaving the
// ringing screen, home included, dismisses the timer.
class TimerScreen final : public Screen {
public:
    void resize(int width,int height) override { width_=width; height_=height; }
    void bind(TimerService* timer,TimerPreferences* preferences) { timer_=timer; preferences_=preferences; }
    bool available() const override { return timer_ && preferences_; }
    void enter(TimeUs now) override;
    void exit() override;
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    TimeUs nextUpdate() const override;
    bool tick(TimeUs now) override;
    TimeUs holdPanelUntil() const override;
    uint8_t vibration() const override { return vibration_; }
    const TimerModel& model() const { return model_; }
private:
    // The view from the service, and what it shows now: the seconds, the
    // RESET fill, the motor, and when each changes next.
    void sample(TimeUs now);
    void setupFrom(int32_t seconds);
    bool start(TimeUs now,ScreenOutcome& out);
    void reset();
    // RESET held for 600ms, by A or a finger: reset now, and spend the rest.
    void resetByHold();
    void togglePause(TimeUs now);
    bool handleSetup(const Events& e,TimeUs now,ScreenOutcome& out);
    bool handleCountdown(const Events& e,TimeUs now);
    TimerService* timer_=nullptr;
    TimerPreferences* preferences_=nullptr;
    TimerModel model_{};
    int width_=468,height_=468;
    bool shown_=false;          // Between enter() and exit().
    bool fresh_=true;           // Entered: the next sample starts its view anew.
    HoldRepeat hold_;           // B held on a setup field.
    TimeUs fillSince_=-1;       // RESET held, by A or a finger, since; -1: not.
    bool touchOnReset_=false;   // The finger now down started on RESET.
    // What is still down after a hold reset the timer: its release, or the
    // rest of the touch, belongs to the countdown it ended, not to the setup
    // that replaced it.
    bool spentA_=false,spentTouch_=false;
    TimeUs ringStart_=-1;       // When the ringing view was shown.
    TimeUs nextShown_=INT64_MAX,nextAlert_=INT64_MAX,nextFill_=INT64_MAX;
    uint8_t vibration_=0;
};
}
