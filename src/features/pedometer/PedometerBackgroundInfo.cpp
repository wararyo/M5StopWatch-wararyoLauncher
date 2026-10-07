#include "PedometerBackgroundInfo.h"
#include "assets/AppIcons.h"
#include <cstdio>
namespace launcher {
void formatPedometerBackground(uint32_t steps,char* label,size_t size) {
    if (steps<100000) std::snprintf(label,size,"%u.%uK",unsigned(steps/1000),unsigned(steps%1000/100));
    else std::snprintf(label,size,"%luK",static_cast<unsigned long>(steps/1000));
}
bool PedometerBackgroundInfo::sample(TimeUs,BackgroundInfo& out) const {
    if (!service_.available()) return false;
    const uint32_t steps=service_.today();
    if (steps<PedometerLineSteps) return false;
    formatPedometerBackground(steps,out.label,sizeof(out.label));
    // The launcher's pedometer mask and colour, shared rather than copied.
    out.icon=appIcon(IconId::Pedometer);
    out.suggestedColor=PedometerColors.background;
    return true;
}
}
