#pragma once
#include "input/InputController.h"
#include "power/PowerManager.h"
#include "services/CivilTime.h"
namespace launcher {
class Hal {
public:
    virtual ~Hal() = default;
    virtual TimeUs now() = 0;
    virtual InputSnapshot sampleInput() = 0;
    virtual UsbState sampleUsb() = 0;
    virtual void setScreenOff(bool off) = 0;
    virtual void waitUs(TimeUs delay) = 0;
    // True once after a press or touch interrupt (or a slot result) ended a
    // wait, so the input is read now instead of at a far deadline (work 8-4).
    virtual bool inputPending() = 0;
    // True once per wrist raised to look at the watch (the IMU's wrist-wear
    // wake-up). Also clears the interrupt, so ask it every step. The fakes
    // without an IMU inherit false.
    virtual bool takeWristWake() { return false; }
    // True when an interrupt reports USB power coming and going, so VBUS need
    // not be polled while the watch sleeps. Then takeUsbEvent() is true once
    // after the line was raised, by that or by anything sharing it: VBUS is
    // read afresh rather than trusting what the event said. The fakes poll.
    virtual bool usbEvents() const { return false; }
    virtual bool takeUsbEvent() { return false; }
    // Automatic light sleep is only safe with the panel asleep (the display
    // driver's DMA transfers hold no PM lock) and without USB power (sleep
    // stops the USB console). Only the runtime test observes it, so the other
    // fakes inherit the no-op (work 8-5).
    virtual void setLightSleepAllowed(bool) {}
    // When the panel woken last starts to show its image. A level set before
    // then is not seen, so a wake's fade starts there. The fakes show at once.
    virtual TimeUs panelShowsAt() const { return 0; }
    // The green status LED, which M5PM1 lights at power-on. The runtime
    // decides when; fakes without one inherit the no-op. False when the
    // write did not reach M5PM1, so the runtime sends it again.
    virtual bool setStatusLed(bool) { return true; }
    // The vibration motor, 0 (off) to 255. False when the level did not reach
    // the motor's driver, so the runtime tries again. Fakes without a motor
    // inherit the no-op.
    virtual bool setVibration(uint8_t level) { (void)level; return true; }
    // The RTC is read and written as UTC (plan.md 7.3). The system clock is
    // what the launcher actually reads each frame, so it sits behind the HAL
    // too: settimeofday is not available on the PC toolchain, and routing it
    // here keeps TimeService runnable in the host tests.
    virtual bool readRtc(CivilTime& utc) = 0;
    virtual bool writeRtc(const CivilTime& utc) = 0;
    virtual void setUtcClock(int64_t unixSeconds) = 0;
    virtual int64_t utcClockUs() = 0;
    virtual BatteryState sampleBattery() = 0;
    virtual void setBrightness(int level) = 0;
};
}
