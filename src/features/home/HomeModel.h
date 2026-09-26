#pragma once
#include "core/Time.h"
#include "features/background/BackgroundInfo.h"
#include <ctime>
#include <cstdint>
namespace launcher {
struct WatchData {
    std::tm localTime{};
    bool timeValid=false;
    int batteryPercent=-1;
    bool charging=false;
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
}
