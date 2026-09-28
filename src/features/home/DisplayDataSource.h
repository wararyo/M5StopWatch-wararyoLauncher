#pragma once
#include "features/home/HomeModel.h"
namespace launcher {
class DisplayDataSource {
public:
    virtual ~DisplayDataSource()=default;
    virtual WatchData sample(TimeUs) { return {}; }
    virtual TimeUs nextUpdate(TimeUs) const { return INT64_MAX; }
    // USB power came or went: the battery is read again on the next frame
    // rather than at its period, so charging shows within about a second.
    virtual void refreshBattery() {}
};
}
