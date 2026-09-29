#pragma once
#include "core/Time.h"
#include "features/background/BackgroundInfo.h"
#include <algorithm>
#include <ctime>
#include <cstdint>
namespace launcher {
struct WatchData {
    std::tm localTime{};
    bool timeValid=false;
    int batteryPercent=-1;
    bool charging=false;
    // Whether `charging` was actually read; false is not "discharging".
    bool chargingKnown=true;
    TimeUs subsecondUs=0;
    // Owned copies of the applications' labels, fixed for the frame. Only
    // collected while the clock is on screen (host/HostRuntime.cpp).
    BackgroundSnapshot background{};
};
inline TimeUs nextMinute(TimeUs now,const WatchData& data) {
    if (!data.timeValid || data.localTime.tm_sec<0 || data.localTime.tm_sec>59 ||
        data.subsecondUs<0 || data.subsecondUs>=1000000) return INT64_MAX;
    return now+(60-data.localTime.tm_sec)*1000000LL-data.subsecondUs;
}
inline TimeUs nextSecond(TimeUs now,const WatchData& data) {
    if (!data.timeValid || data.subsecondUs<0 || data.subsecondUs>=1000000) return INT64_MAX;
    return now+1000000-data.subsecondUs;
}
// The next :00, :10 ... :50.
// A leap second counts as :59, whose next boundary is the same :00.
inline TimeUs nextTenSeconds(TimeUs now,const WatchData& data) {
    if (!data.timeValid || data.localTime.tm_sec<0 || data.localTime.tm_sec>60 ||
        data.subsecondUs<0 || data.subsecondUs>=1000000) return INT64_MAX;
    const int second=std::min(data.localTime.tm_sec,59);
    return now+(10-second%10)*1000000LL-data.subsecondUs;
}
}
