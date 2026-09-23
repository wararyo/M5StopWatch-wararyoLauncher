#pragma once
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
