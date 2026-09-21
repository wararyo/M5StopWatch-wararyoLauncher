#include "LauncherData.h"
namespace launcher {
WatchData LauncherData::sample(TimeUs now) {
    // Only ever called from the draw path, so a dark panel costs no I2C: the
    // runtime stops drawing before it stops sampling, and the stale deadline
    // makes the first frame after a wake read fresh.
    if (now >= batteryDue_) {
        battery_ = hal_.sampleBattery();
        batteryDue_ = now + BatteryPeriodUs;
    }
    WatchData data{};
    data.timeValid = time_.now(data.localTime, data.subsecondUs);
    data.batteryPercent = battery_.percent;
    data.charging = battery_.charging;
    return data;
}
TimeUs LauncherData::nextUpdate(TimeUs) const { return batteryDue_; }
}
