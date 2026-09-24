#include "features/stopwatch/StopwatchModel.h"
#include <cstdio>
namespace launcher {
void formatStopwatch(TimeUs elapsed,char* clock,size_t clockSize,
                     char* fraction,size_t fractionSize) {
    if (elapsed<0) elapsed=0;
    if (elapsed>StopwatchDisplayCapUs) elapsed=StopwatchDisplayCapUs;
    const int64_t hundredths=elapsed/10000;
    std::snprintf(clock,clockSize,"%02d:%02d:%02d",int(hundredths/360000),
                  int((hundredths/6000)%60),int((hundredths/100)%60));
    std::snprintf(fraction,fractionSize,".%02d",int(hundredths%100));
}
}
