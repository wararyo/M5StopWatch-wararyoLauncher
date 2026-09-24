#pragma once
#include "Hal.h"
namespace launcher {
class M5Hal final : public Hal {
public:
    TimeUs now() override;
    InputSnapshot sampleInput() override;
    UsbState sampleUsb() override;
    void setScreenOff(bool off) override;
    void waitUs(TimeUs delay) override;
    bool inputPending() override;
    void setLightSleepAllowed(bool allowed) override;
    // Call on the UI task: it is the task the interrupts notify.
    void beginInputWake();
    bool readRtc(CivilTime& utc) override;
    bool writeRtc(const CivilTime& utc) override;
    void setUtcClock(int64_t unixSeconds) override;
    int64_t utcClockUs() override;
    BatteryState sampleBattery() override;
    void setBrightness(int level) override;
private:
    bool inputWake_ = false, pending_ = false;
};
// Dynamic frequency scaling between the two limits, and automatic light sleep
// while setLightSleepAllowed(true) (work 8-5).
void beginPowerManagement(int maxMhz, int minMhz);
void beginRuntimeDiagnostics();
void runtimeDiagnostics();
}
