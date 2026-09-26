#include "StopwatchBackgroundInfo.h"
#include <cinttypes>
#include <cstdio>
namespace launcher {
namespace {
constexpr TimeUs Second=1000000,Minute=60*Second,Hour=60*Minute;
}
TimeUs formatStopwatchBackground(TimeUs elapsed,char* label,size_t size) {
    if (elapsed<0) elapsed=0;
    // Truncated, never rounded, and the unit changes with the period: 59:59 is
    // followed by 01:00. The hours are not capped, so 100 hours read 100:00.
    if (elapsed<Hour) {
        std::snprintf(label,size,"%02d:%02d",int(elapsed/Minute),int(elapsed/Second%60));
        return Second-elapsed%Second;
    }
    std::snprintf(label,size,"%02" PRId64 ":%02d",int64_t(elapsed/Hour),int(elapsed/Minute%60));
    return Minute-elapsed%Minute;
}
bool StopwatchBackgroundInfo::sample(TimeUs now,BackgroundInfo& out) const {
    if (service_.state()!=StopwatchState::Running) return false;
    const TimeUs wait=formatStopwatchBackground(service_.elapsed(now),out.label,sizeof(out.label));
    // The next boundary after now, even when this frame is late; saturated
    // rather than wrapped at the end of the monotonic range.
    out.nextChangeAt=now>INT64_MAX-wait ? INT64_MAX : now+wait;
    return true;
}
}
