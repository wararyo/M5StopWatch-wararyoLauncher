#pragma once
#include "features/background/BackgroundInfo.h"
#include "services/StopwatchService.h"
#include <cstddef>
namespace launcher {
// The stopwatch's line on the watch face (docs/task10/plan.md 4.2): only while
// running, `mm:ss` under an hour and `HH:mm` from then on. It reads the service
// and nothing else, so it keeps working with the stopwatch screen closed, and
// its deadlines follow the measurement, not the wall clock.
class StopwatchBackgroundInfo final : public BackgroundInfoProvider {
public:
    explicit StopwatchBackgroundInfo(const StopwatchService& service):service_(service) {}
    LaunchTargetId id() const override { return LaunchTargetId::Stopwatch; }
    bool sample(TimeUs now,BackgroundInfo& out) const override;
private:
    const StopwatchService& service_;
};
// The label for `elapsed` and how long until it next changes. Split out so the
// format and the boundaries can be checked without a running service.
TimeUs formatStopwatchBackground(TimeUs elapsed,char* label,size_t size);
}
