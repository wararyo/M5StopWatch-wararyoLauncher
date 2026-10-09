#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
namespace launcher {
// What the pedometer screen shows (docs/task13/plan.md 1.3): today's steps, or
// nothing to count when there is no IMU that reads.
struct PedometerModel {
    bool available=false;
    uint32_t steps=0;
};
// The count with a comma every three digits, in both languages ("12,345"),
// or "--" when there is nothing to count.
inline void formatSteps(const PedometerModel& m,char* out,size_t size) {
    if (size==0) return;
    if (!m.available) { std::snprintf(out,size,"--"); return; }
    char digits[12];
    const int length=std::snprintf(digits,sizeof(digits),"%lu",static_cast<unsigned long>(m.steps));
    size_t at=0;
    for (int i=0;i<length && at+1<size;++i) {
        if (i>0 && (length-i)%3==0 && at+2<size) out[at++]=',';
        out[at++]=digits[i];
    }
    out[at]=0;
}
}
