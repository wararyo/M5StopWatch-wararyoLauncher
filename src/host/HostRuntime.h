#pragma once
#include "hal/Hal.h"
#include "host/ScreenManager.h"
#include "host/RenderPort.h"
#include "features/home/DisplayDataSource.h"
#include "features/background/BackgroundInfoHub.h"
#include "host/AttentionSource.h"
#include <algorithm>
namespace launcher {
// The single UI task's loop: input, power, deadlines, slot results and when to
// draw. It drives the screens it is lent and owns none of the application's
// state (host/HostApplication.h owns it).
class HostRuntime {
public:
    // Without a hub the clock gets an empty background snapshot.
    HostRuntime(Hal& hal, RenderPort& renderer, DisplayDataSource& data, ScreenManager& screens,
                BackgroundInfoHub* background = nullptr)
        : hal_(hal), renderer_(renderer), data_(data), background_(background),
          input_(std::max(1, std::min(screens.viewport().width, screens.viewport().height) / 50)),
          screens_(screens) {}
    HostRuntime(const HostRuntime&) = delete;
    HostRuntime& operator=(const HostRuntime&) = delete;
    void bindSlots(SlotService& slots) { slots_=&slots; screens_.bindSlots(&slots); }
    // A feature that may ask for the wearer's attention (host/AttentionSource.h).
    // Asked every step before input and waited for with the panel dark too.
    // Sources are asked in registration order, so when several start in the
    // same step the last one's screen is what stays shown. False when full.
    bool bindAttention(AttentionSource& source) {
        if (attentionCount_ >= AttentionCapacity) return false;
        attention_[attentionCount_++] = &source; return true;
    }
    // A service that counts through the dark (services/ClockFollower.h). The
    // clock is aligned with the RTC when it asks, with the panel dark too, and
    // it is handed what every alignment finds. False when full.
    bool bindClockFollower(ClockFollower& follower) {
        if (followerCount_ >= FollowerCapacity) return false;
        followers_[followerCount_++] = &follower; return true;
    }
    void begin();
    void step();
    // `also` is a deadline of the caller's own, such as an instrument's.
    void wait(TimeUs also = INT64_MAX);
    const PowerManager& power() const { return power_; }
    FrameModel model() const { return screens_.model(); }
    void dataChanged() { dirty_ = true; } // UI-task service notification
    // Round up in HAL to ticks; even an overrun must give idle a chance.
    static TimeUs waitDelay(TimeUs now, TimeUs deadline) { return std::max<TimeUs>(1000, deadline - now); }
private:
    Hal& hal_;
    RenderPort& renderer_;
    DisplayDataSource& data_;
    BackgroundInfoHub* background_;
    SlotService* slots_=nullptr;
    static constexpr int AttentionCapacity = 4;
    AttentionSource* attention_[AttentionCapacity]{};
    bool attended_[AttentionCapacity]{}; // Started, and still asking.
    int attentionCount_ = 0;
    static constexpr int FollowerCapacity = 4;
    ClockFollower* followers_[FollowerCapacity]{};
    int followerCount_ = 0;
    // Begins aligning the clock with the RTC, and tells every follower so.
    void alignClock(TimeUs now);
    // The earliest any follower wants the clock aligned.
    TimeUs alignmentDue() const;
    InputController input_;
    ScreenManager& screens_;
    PowerManager power_;
    TimeUs nextInput_ = 0, nextUsb_ = 0;
    // VBUS is still polled this long after a USB power event.
    static constexpr TimeUs UsbSettleUs = 5000000;
    TimeUs usbSettleUntil_ = 0;
    bool sleepAllowed() const;
    bool usbPolled(TimeUs now) const;
    // How long input is still read at its period after an interrupt.
    static constexpr TimeUs FollowUs = 100000;
    TimeUs followUntil_ = 0;
    bool lightSleep_ = false; // The HAL starts with light sleep forbidden.
    int statusLed_ = -1; // Unknown until the first charge reading sets it.
    TimeUs nextDisplay_ = INT64_MAX;
    TimeUs nextService_ = INT64_MAX; // The data source's own work (DisplayDataSource::service).
    // A wake fades the level in from zero, eased out, stepped at a frame rate.
    static constexpr TimeUs FadeUs = 300000, FadeStepUs = 16000;
    TimeUs fadeEnd_ = 0; // Zero once the full level is applied.
    int brightness_ = Settings{}.brightness; // The level to reach, from the last draw.
    int appliedBrightness_ = -1; // Forced re-apply after every wake.
    void applyBrightness(TimeUs now);
    // Asks every source and starts the requests that began: before the
    // step's input, the held input is spent, the panel lit, the screen shown.
    void attend(TimeUs now);
    // Holds the panel and sets the motor as the shown screen asks.
    void followScreen(TimeUs now);
    // Sets the motor to the wanted level, retrying a write that did not land.
    void applyVibration(TimeUs now);
    // The level the shown screen wants and the one the motor was last set to.
    // They differ only until a write lands; a failed one is retried shortly,
    // even once nothing asks any more, so a motor is never left running.
    int vibration_ = 0, appliedVibration_ = 0;
    TimeUs vibrationRetry_ = 0;
    static constexpr TimeUs VibrationRetryUs = 50000;
    TimeUs nextVibration() const { return vibration_ != appliedVibration_ ? vibrationRetry_ : INT64_MAX; }
    bool dirty_ = true;
    bool scanRequested_ = false; // The scan starts behind the first frame.
};
}
