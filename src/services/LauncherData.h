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
class LauncherData final : public DisplayDataSource {
public:
    LauncherData(Hal& hal, TimeService& time) : hal_(hal), time_(time) {}
    WatchData sample(TimeUs now) override;
    TimeUs nextUpdate(TimeUs now) const override;
private:
    Hal& hal_;
    TimeService& time_;
    BatteryState battery_{};
    TimeUs batteryDue_ = INT64_MIN; // Read on the first frame, then every period.
};
}
