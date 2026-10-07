#include "PedometerService.h"
#include <cstdio>
namespace launcher {
TimeUs pedometerAlignmentLeft(TimeUs left) {
    constexpr TimeUs Minute=60000000;
    if (left>10*Minute) return 10*Minute;
    if (left>Minute) return Minute;
    return 0;
}
void PedometerService::begin(TimeUs now,int64_t restoredDay,uint32_t restoredSteps,bool restored) {
    refresh(now);
    if (!clockSet()) return;
    startDay(now,clockDay());
    if (restored && restoredDay==day_) carried_=restoredSteps;
}
bool PedometerService::refresh(TimeUs now) {
    uint32_t count=0;
    if (!hal_.readStepCount(count)) return false;
    // Less than before: the IMU began again and counts from 0. What it had
    // counted today stays today's.
    if (count<last_) { carried_+=last_-dayStart_; dayStart_=0; }
    last_=count;
    readAt_=now;
    available_=true;
    return true;
}
bool PedometerService::refreshIfOlder(TimeUs now,TimeUs age) {
    if (readAt_!=INT64_MIN && now-readAt_<age) return false;
    return refresh(now);
}
TimeUs PedometerService::service(TimeUs now,bool& changed) {
    changed=false;
    if (!clockSet()) return INT64_MAX;
    const int64_t day=clockDay();
    if (!dated_) {
        // Set for the first time: the steps counted until now are the set
        // day's (docs/task13/plan.md 1.1).
        dated_=true; day_=day; alignLeft_=pedometerAlignmentLeft(dayEnd(now)-now);
        changed=true;
    } else if (day!=day_) {
        if (now<retryAt_) return retryAt_;
        // Up to now the steps are the old day's, so the count is read first.
        if (!refresh(now) && ++failures_<Attempts) { retryAt_=now+RetryUs; return retryAt_; }
        const uint32_t ended=today();
        startDay(now,day);
        carried_=0;
        changed=true;
        std::printf("[Pedometer] day=%lld began (the last ended with %u steps)\n",
                    static_cast<long long>(day_),unsigned(ended));
    }
    return dayEnd(now);
}
TimeUs PedometerService::alignmentDue() const {
    if (!dated_ || alignLeft_<=0) return INT64_MAX;
    return dayEnd(hal_.now())-alignLeft_;
}
void PedometerService::alignmentBegun(TimeUs now) {
    if (!dated_) return;
    const TimeUs end=dayEnd(now);
    // Begun for the point it was planned at, however late the loop got here,
    // so the next one follows from that point; begun early (the panel came
    // on), from what is left now.
    const TimeUs left=alignLeft_>0 && now>=end-alignLeft_ ? alignLeft_ : end-now;
    alignLeft_=pedometerAlignmentLeft(left);
}
int64_t PedometerService::clockDay() const {
    const int64_t us=hal_.utcClockUs();
    return pedometerDay(us/1000000-(us%1000000<0 ? 1 : 0));
}
TimeUs PedometerService::monotonicAt(TimeUs now,int64_t unixSeconds) const {
    return now+(unixSeconds*1000000-hal_.utcClockUs());
}
void PedometerService::startDay(TimeUs now,int64_t day) {
    dated_=true; day_=day;
    dayStart_=last_;
    failures_=0; retryAt_=0;
    alignLeft_=pedometerAlignmentLeft(dayEnd(now)-now);
}
}
