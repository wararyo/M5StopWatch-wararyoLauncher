#include "TimerBackgroundInfo.h"
#include "assets/AppIcons.h"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
namespace launcher {
namespace {
constexpr TimeUs Second=1000000,Minute=60*Second,Hour=60*Minute;
}
TimeUs formatTimerBackground(TimeUs remaining,char* label,size_t size) {
    if (remaining<=0) { std::snprintf(label,size,"00:00"); return INT64_MAX; }
    const TimeUs seconds=remaining/Second+(remaining%Second!=0);
    if (seconds<3600) {
        std::snprintf(label,size,"%02d:%02d",int(seconds/60),int(seconds%60));
        return remaining-(seconds-1)*Second;
    }
    // From an hour on, the minute boundaries; the last one before an hour is
    // the switch to seconds at 59:59, not 59 minutes. The longest timer,
    // 99:59:59, reads 100:00 for its first seconds, as the rule rounds it.
    const TimeUs minutes=seconds/60+(seconds%60!=0);
    std::snprintf(label,size,"%02" PRId64 ":%02d",int64_t(minutes/60),int(minutes%60));
    return remaining-std::max((minutes-1)*Minute,Hour-Second);
}
bool TimerBackgroundInfo::sample(TimeUs now,BackgroundInfo& out) const {
    if (service_.state()!=TimerState::Running) return false;
    // Run out but not yet noticed: the alert is about to take the screen.
    const TimeUs left=service_.remaining(now);
    if (left<=0) return false;
    const TimeUs wait=formatTimerBackground(left,out.label,sizeof(out.label));
    out.nextChangeAt=now>INT64_MAX-wait ? INT64_MAX : now+wait;
    // The launcher's timer mask and colour, shared rather than copied.
    out.icon=appIcon(IconId::Timer);
    out.suggestedColor=TimerColors.background;
    return true;
}
}
