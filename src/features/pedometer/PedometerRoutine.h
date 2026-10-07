#pragma once
#include "host/HostRoutine.h"
#include "services/PedometerService.h"
#include "storage/PedometerRecord.h"
namespace launcher {
// When the pedometer reads, ends its day and saves (docs/task13/plan.md 2.4,
// 2.5), so the service only counts and the record only stores:
// - at start, today's steps carry over from a record of the same day;
// - every step, the day ends at 04:00, with the panel dark too;
// - a frame that draws the clock reads the count when it is ClockReadUs old,
//   so the watch face's label follows without the clock being woken for it;
// - going dark, and an external boot, read and save today's steps.
// The pedometer screen reads on its own deadline.
class PedometerRoutine final : public HostRoutine {
public:
    static constexpr TimeUs ClockReadUs=30000000;
    PedometerRoutine(PedometerService& service,PedometerRecord& record):service_(service),record_(record) {}
    void begin(TimeUs now) override;
    TimeUs service(TimeUs now,bool& changed) override { return service_.service(now,changed); }
    void panelOff(TimeUs now) override { save(now); }
    void beforeClock(TimeUs now) override { service_.refreshIfOlder(now,ClockReadUs); }
    // Reads the count and writes today's steps if they moved since the last
    // save. Nothing is written while the date is unknown: the steps belong to
    // no day yet. A failed write is only logged; the next save tries again.
    PrefResult save(TimeUs now);
private:
    PedometerService& service_;
    PedometerRecord& record_;
};
}
