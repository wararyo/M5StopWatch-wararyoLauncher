#pragma once
#include "services/TimeService.h"
#include "features/home/DisplayDataSource.h"
namespace launcher {
// The battery is read at most this often: the face shows whole percent, and the
// read is an I2C transaction the UI task pays for (plan.md 6.2). The clock's own
// minute boundary comes from the watch face, not from here.
constexpr TimeUs BatteryPeriodUs = 30000000;
// The single place where the watch face's data stops being injected and starts
// coming from the device. Nothing below the display boundary reads the RTC or
// the PMIC directly, so the diagnostics build can still substitute its own.
class HomeDataSource final : public DisplayDataSource {
public:
    HomeDataSource(Hal& hal, TimeService& time) : hal_(hal), time_(time) {}
    WatchData sample(TimeUs now) override;
    TimeUs nextUpdate(TimeUs now) const override;
    // Still read only on the draw path: a dark panel costs no I2C for it.
    void refreshBattery() override { batteryDue_ = INT64_MIN; }
    // The system clock is aligned with the RTC after every dark spell, and
    // when a timer or the stopwatch asks.
    void alignClock(TimeUs now) override { time_.beginAlign(now); }
    TimeUs service(TimeUs now, ClockAlignment& alignment) override { return time_.align(now, alignment); }
private:
    Hal& hal_;
    TimeService& time_;
    BatteryState battery_{};
    TimeUs batteryDue_ = INT64_MIN; // Read on the first frame, then every period.
};
}
