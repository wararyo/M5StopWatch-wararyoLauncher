#pragma once
#include "Hal.h"
#include "power/BatteryCurve.h"
namespace launcher {
class M5Hal final : public Hal {
public:
    TimeUs now() override;
    InputSnapshot sampleInput() override;
    UsbState sampleUsb() override;
    void setScreenOff(bool off) override;
    void waitUs(TimeUs delay) override;
    bool inputPending() override;
    bool takeWristWake() override;
    void setLightSleepAllowed(bool allowed) override;
    bool setStatusLed(bool on) override;
    // Call on the UI task: it is the task the interrupts notify. Sets up the
    // IMU's wrist-wear wake-up first, since its line is one of them.
    void beginInputWake();
    bool readRtc(CivilTime& utc) override;
    bool writeRtc(const CivilTime& utc) override;
    void setUtcClock(int64_t unixSeconds) override;
    int64_t utcClockUs() override;
    BatteryState sampleBattery() override;
    void setBrightness(int level) override;
private:
    bool inputWake_ = false, pending_ = false, imuWake_ = false, imuRetry_ = false;
    // USB power as of the last valid VBUS read, for the estimator.
    bool usbPowered_ = false;
    BatteryEstimator battery_;
};
// Dynamic frequency scaling between the two limits, and automatic light sleep
// while setLightSleepAllowed(true) (work 8-5).
void beginPowerManagement(int maxMhz, int minMhz);
void beginRuntimeDiagnostics();
void runtimeDiagnostics();
}
