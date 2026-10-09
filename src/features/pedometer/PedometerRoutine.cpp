#include "PedometerRoutine.h"
#include <cstdio>
namespace launcher {
void PedometerRoutine::begin(TimeUs now) {
    service_.begin(now,record_.day(),record_.steps(),record_.has());
    std::printf("[Pedometer] begin imu=%s dated=%d day=%lld steps=%u record=%s day=%ld steps=%lu\n",
                service_.available() ? "ok" : "none",int(service_.dated()),static_cast<long long>(service_.day()),
                unsigned(service_.today()),record_.has() ? "yes" : "no",long(record_.day()),
                static_cast<unsigned long>(record_.steps()));
}
PrefResult PedometerRoutine::save(TimeUs now) {
    // A save that lands on 04:00 (a boot committed then, say) ends the day
    // first, so today's steps are kept under today's number.
    bool changed=false;
    service_.service(now,changed);
    service_.refresh(now);
    if (!service_.dated()) return PrefResult::Unavailable;
    const int32_t day=int32_t(service_.day());
    const uint32_t steps=service_.today();
    const bool moved=!record_.has() || record_.day()!=day || record_.steps()!=steps;
    const auto result=record_.remember(day,steps);
    if (result!=PrefResult::Ok) std::printf("[Pedometer] save failed: %s\n",prefResultName(result));
    else if (moved) std::printf("[Pedometer] saved day=%ld steps=%lu\n",long(day),static_cast<unsigned long>(steps));
    return result;
}
}
