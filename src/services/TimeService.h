#pragma once
#include "hal/Hal.h"
#include <ctime>
namespace launcher {
enum class SaveResult { Saved, Invalid, RtcWriteFailed, Unrecoverable };
class TimeService {
public:
    // The RTC itself holds 1900..2099. The launcher only trusts a window it
    // could plausibly have been set to, so a cleared or garbage RTC reads as
    // invalid and the watch face shows `--:--` instead of the year 1900.
    static constexpr int MinYear = 2024, MaxYear = 2099;
    static bool trusted(const CivilTime& c) {
        return c.year >= MinYear && c.year <= MaxYear && validCivil(c);
    }
    bool begin(Hal& hal);
    bool valid() const { return valid_; }
    // JST, for display only. Reads the system clock, never the RTC, so it is
    // cheap enough for every frame (plan.md 7.3).
    bool now(std::tm& jst, TimeUs& subsecondUs) const;
    SaveResult save(const CivilTime& jstInput);
private:
    bool syncFromRtc();
    Hal* hal_ = nullptr;
    bool valid_ = false;
};
}
