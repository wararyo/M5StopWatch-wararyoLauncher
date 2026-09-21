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
    bool readRtc(CivilTime& utc) override;
    bool writeRtc(const CivilTime& utc) override;
    void setUtcClock(int64_t unixSeconds) override;
    int64_t utcClockUs() override;
    BatteryState sampleBattery() override;
    void setBrightness(int level) override;
};
void beginRuntimeDiagnostics();
void runtimeDiagnostics();
}
