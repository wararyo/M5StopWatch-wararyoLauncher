#pragma once
#include "features/background/BackgroundInfo.h"
#include "services/PedometerService.h"
#include <cstddef>
namespace launcher {
// The pedometer's line on the watch face (docs/task13/plan.md 1.4): today's
// steps once they reach PedometerLineSteps, in thousands, with the
// pedometer's own icon and colour. Below that, or without an IMU, nothing.
// It reads the service's last count and nothing else; the count is read on
// the clock's own frames (features/pedometer/PedometerRoutine.h), so the line
// has no deadline of its own.
inline constexpr uint32_t PedometerLineSteps=10000;
class PedometerBackgroundInfo final : public BackgroundInfoProvider {
public:
    explicit PedometerBackgroundInfo(const PedometerService& service):service_(service) {}
    LaunchTargetId id() const override { return LaunchTargetId::Pedometer; }
    bool sample(TimeUs now,BackgroundInfo& out) const override;
private:
    const PedometerService& service_;
};
// The label for `steps`, rounded down: tenths of a thousand under 100,000
// (10,099 reads 10.0K, 99,999 99.9K), whole thousands from then on (100K).
void formatPedometerBackground(uint32_t steps,char* label,size_t size);
}
