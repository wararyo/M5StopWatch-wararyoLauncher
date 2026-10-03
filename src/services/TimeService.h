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
    // The system clock runs on the internal RC oscillator through light sleep
    // and gains about 0.02-0.03% there, while the RTC keeps a crystal. The RTC
    // counts whole seconds, so it tells the time to the millisecond only as
    // its seconds change: alignment reads it until they do and sets the
    // system clock at that edge. One RTC read per align() call, so the caller
    // keeps drawing; align() returns when it is next due (INT64_MAX: done) and
    // sets `stepped` when it set the clock. Gives up after AlignWindowUs. An
    // edge is taken only from a read within AlignGapUs of the previous one:
    // after a slow frame it may lie that far back and would set the clock
    // late by as much.
    static constexpr TimeUs AlignPollUs = 5000, AlignGapUs = 20000, AlignWindowUs = 1500000;
    void beginAlign(TimeUs now);
    TimeUs align(TimeUs now, bool& stepped);
private:
    bool syncFromRtc();
    Hal* hal_ = nullptr;
    bool valid_ = false;
    TimeUs alignUntil_ = INT64_MIN; // In progress until then.
    bool alignFirst_ = false;
    int64_t alignSecond_ = 0; // The RTC's second before the edge.
    TimeUs alignReadAt_ = 0; // When alignSecond_ was read.
};
}
