#pragma once
#include "hal/Hal.h"
#include "services/ClockFollower.h"
#include "services/TimeService.h"
#include <cstdint>
namespace launcher {
// A pedometer day runs from 04:00 JST to 04:00 the next day
// (docs/task13/plan.md 1.1). Its number is the JST date it starts on, counted
// in days from 1970-01-01 like dayFromUnix.
inline constexpr int64_t PedometerDayStartSec=4*3600;
constexpr int64_t pedometerDay(int64_t unixSeconds) {
    return dayFromUnix(unixSeconds+JstOffsetSec-PedometerDayStartSec);
}
// The UTC second a day begins at.
constexpr int64_t pedometerDayStart(int64_t day) {
    return day*86400-JstOffsetSec+PedometerDayStartSec;
}
// Where the clock is next aligned with the RTC, as what is left until the
// day ends then, from what is left now (docs/task13/plan.md 2.3): with ten
// minutes left, then with one. 0 after that: the day ends on the clock as the
// last alignment set it.
TimeUs pedometerAlignmentLeft(TimeUs left);
// Today's steps (docs/task13/plan.md 2.2). The IMU counts on its own from the
// launcher's start; this keeps where today began in that count and what came
// before it (a restart's record, or a count the IMU lost), and ends the day at
// 04:00 by the system clock. It reads the IMU only when asked and knows
// nothing of storage, the screen or the panel: features/pedometer/
// PedometerRoutine decides when.
//
// The day ends by the system clock, which gains in light sleep, so it is a
// ClockFollower: the clock is aligned ten minutes and one minute before the
// day ends. What an alignment measures is not needed, since the end is read
// off the aligned clock.
class PedometerService final : public ClockFollower {
public:
    explicit PedometerService(Hal& hal):hal_(hal) {}
    // Without it the date is never known and days never end.
    void bindTime(const TimeService* time) { time_=time; }
    // At start, after the IMU began: the first read, and the day if the clock
    // is set. A record (`restored`) of that same day carries its steps over
    // the restart; one of another day is left out.
    void begin(TimeUs now,int64_t restoredDay,uint32_t restoredSteps,bool restored);
    // Reads the IMU. False when it could not be read; the count stays.
    bool refresh(TimeUs now);
    // Reads only when the last read is at least `age` old (or never was).
    bool refreshIfOlder(TimeUs now,TimeUs age);
    // The day's bookkeeping, every step: a clock that is newly set, or that
    // reached (or was set to) another day. Returns when it is next due:
    // the end of the day, or a retry after a read that failed there.
    // `changed` when today's count or day moved.
    TimeUs service(TimeUs now,bool& changed);
    // A read has succeeded: there is an IMU that counts.
    bool available() const { return available_; }
    // The clock was set, so today is a day.
    bool dated() const { return dated_; }
    int64_t day() const { return day_; }
    uint32_t today() const { return carried_+(last_-dayStart_); }
    TimeUs lastReadAt() const { return readAt_; }
    TimeUs alignmentDue() const override;
    void alignmentBegun(TimeUs now) override;
    void clockCorrected(TimeUs) override {}
    // A read that fails when the day ends is tried again this often, and the
    // day ends without it after this many.
    static constexpr TimeUs RetryUs=1000000;
    static constexpr int Attempts=3;
private:
    bool clockSet() const { return time_ && time_->valid(); }
    int64_t clockDay() const;
    // The monotonic time at which the system clock reads `unixSeconds`.
    TimeUs monotonicAt(TimeUs now,int64_t unixSeconds) const;
    TimeUs dayEnd(TimeUs now) const { return monotonicAt(now,pedometerDayStart(day_+1)); }
    void startDay(TimeUs now,int64_t day);
    Hal& hal_;
    const TimeService* time_=nullptr;
    bool available_=false,dated_=false;
    int64_t day_=0;
    // The IMU's count: where today began in it, and as last read.
    uint32_t dayStart_=0,last_=0;
    // Today's steps that the count does not hold.
    uint32_t carried_=0;
    TimeUs readAt_=INT64_MIN;
    int failures_=0;
    TimeUs retryAt_=0;
    // What is left until the day ends at the next alignment, 0: none.
    TimeUs alignLeft_=0;
};
}
