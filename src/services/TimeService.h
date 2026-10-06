#pragma once
#include "hal/Hal.h"
#include "services/ClockFollower.h"
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
    // Sets the clock in the middle of a second, so the next align() call
    // starts an alignment of its own (see below).
    SaveResult save(const CivilTime& jstInput);
    // The system clock runs on the internal RC oscillator through light sleep
    // and gains there (0.02-0.03% with frequent wakes, 0.75% over a 20-minute
    // sleep), while the RTC keeps a crystal. The RTC counts whole seconds, so
    // it tells the time to the millisecond only as its seconds change:
    // alignment reads it until they do and sets the system clock at that edge.
    // One RTC read per align() call, so the caller keeps drawing; align()
    // returns when it is next due (INT64_MAX: done) and says in `result` what
    // it did. Gives up after AlignWindowUs. An edge is taken only from a read
    // within AlignGapUs of the previous one, since after a slow frame it may
    // lie that far back and would set the clock late by as much, and only to
    // the next second, so a misread second never sets it.
    //
    // What the clock had gained is measured when it was last set at an edge.
    // Setting it at boot or by hand lands anywhere within a second, so the
    // alignment after that measures nothing, and save() has one start at once,
    // while the panel is lit and nothing drifts.
    static constexpr TimeUs AlignPollUs = 5000, AlignGapUs = 20000, AlignWindowUs = 1500000;
    void beginAlign(TimeUs now);
    TimeUs align(TimeUs now, ClockAlignment& result);
private:
    bool syncFromRtc();
    // Sets the clock wherever the second has got to.
    void setOffEdge(int64_t unixSeconds);
    Hal* hal_ = nullptr;
    bool valid_ = false;
    TimeUs alignUntil_ = INT64_MIN; // In progress until then.
    bool alignFirst_ = false;
    bool alignWanted_ = false; // Begun by the next align() call.
    int64_t alignSecond_ = 0; // The RTC's second before the edge.
    TimeUs alignReadAt_ = 0; // When alignSecond_ was read.
    bool edgeSet_ = false; // The clock was last set at an edge,
    TimeUs edgeAt_ = 0;    // then.
};
}
