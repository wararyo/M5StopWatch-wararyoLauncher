#include "TimeService.h"
#include <cstdio>
namespace launcher {
bool TimeService::syncFromRtc() {
    CivilTime utc{};
    if (!hal_ || !hal_->readRtc(utc) || !trusted(utc)) { valid_ = false; return false; }
    setOffEdge(unixFromCivil(utc));
    valid_ = true;
    return true;
}
void TimeService::setOffEdge(int64_t unixSeconds) {
    hal_->setUtcClock(unixSeconds);
    edgeSet_ = false;
}
bool TimeService::begin(Hal& hal) {
    hal_ = &hal;
    const bool ok = syncFromRtc();
    CivilTime utc{};
    const bool read = hal.readRtc(utc);
    std::printf("[Time] rtc_read=%s utc=%04d-%02d-%02d %02d:%02d:%02d trusted=%s\n",
                read ? "ok" : "FAILED", utc.year, utc.month, utc.day,
                utc.hour, utc.minute, utc.second, ok ? "yes" : "no");
    return ok;
}
bool TimeService::now(std::tm& jst, TimeUs& subsecondUs) const {
    if (!valid_ || !hal_) return false;
    const int64_t us = hal_->utcClockUs();
    int64_t seconds = us / 1000000, rest = us % 1000000;
    if (rest < 0) { rest += 1000000; --seconds; }
    subsecondUs = rest;
    tmFromUnix(seconds + JstOffsetSec, jst);
    return true;
}
SaveResult TimeService::save(const CivilTime& jstInput) {
    if (!hal_) return SaveResult::Unrecoverable;
    CivilTime jst = jstInput;
    jst.second = 0; // Manual entry edits to the minute; seconds start at zero (5.4).
    if (!trusted(jst)) return SaveResult::Invalid;
    // Whichever way it goes, the clock ends up set within a second, or as it
    // was: aligned at once, it measures drift again from the next alignment.
    alignWanted_ = true;
    const int64_t target = unixFromCivil(jst) - JstOffsetSec;
    if (!hal_->writeRtc(civilFromUnix(target))) {
        // Nothing reached the chip. Recover whatever the RTC still holds so the
        // display and the system clock stay consistent with each other.
        const bool recovered = syncFromRtc();
        std::printf("[Time] rtc write failed; recovered=%s\n", recovered ? "yes" : "no");
        return recovered ? SaveResult::RtcWriteFailed : SaveResult::Unrecoverable;
    }
    CivilTime readback{};
    // The write cannot report failure, so the read-back is the only proof that
    // it stuck. The RTC may have ticked past the written second in between, so
    // a couple of seconds ahead of the target still counts as the same write.
    const bool read = hal_->readRtc(readback) && trusted(readback);
    const int64_t drift = read ? unixFromCivil(readback) - target : 0;
    if (!read || drift < 0 || drift > 2) {
        const bool recovered = syncFromRtc();
        std::printf("[Time] rtc read-back mismatch; recovered=%s\n", recovered ? "yes" : "no");
        return recovered ? SaveResult::RtcWriteFailed : SaveResult::Unrecoverable;
    }
    setOffEdge(unixFromCivil(readback));
    valid_ = true;
    return SaveResult::Saved;
}
void TimeService::beginAlign(TimeUs now) {
    if (!hal_) return;
    alignUntil_ = now + AlignWindowUs;
    alignFirst_ = true;
}
TimeUs TimeService::align(TimeUs now, ClockAlignment& result) {
    result = {};
    if (alignWanted_) { alignWanted_ = false; beginAlign(now); }
    if (alignUntil_ == INT64_MIN) return INT64_MAX;
    CivilTime utc{};
    // A failed or untrusted read leaves the clock as it is; the next wake
    // tries again.
    if (now > alignUntil_ || !hal_->readRtc(utc) || !trusted(utc)) {
        std::printf("[Time] rtc alignment %s\n", now > alignUntil_ ? "timed out" : "read failed");
        alignUntil_ = INT64_MIN;
        return INT64_MAX;
    }
    const int64_t second = unixFromCivil(utc);
    // A late read starts the watch again from itself instead, and so does
    // any second but the next: a misread, or a gap the edge may lie inside.
    const bool late = now - alignReadAt_ > AlignGapUs;
    alignReadAt_ = now;
    if (alignFirst_ || late || second != alignSecond_ + 1) {
        alignFirst_ = false;
        alignSecond_ = second;
        return now + AlignPollUs;
    }
    // The edge came since the previous read, at most AlignGapUs ago.
    const int64_t before = hal_->utcClockUs();
    hal_->setUtcClock(second);
    valid_ = true;
    alignUntil_ = INT64_MIN;
    result.stepped = true;
    const TimeUs gain = before - second * 1000000;
    if (edgeSet_) {
        result.measured = true;
        result.gainUs = gain;
        const TimeUs over = now - edgeAt_;
        std::printf("[Time] aligned to rtc: system clock was %+lldms over %llds (%+lldppm)\n",
                    (long long)(gain / 1000), (long long)(over / 1000000),
                    (long long)(over > 0 ? gain * 1000000 / over : 0));
    } else {
        std::printf("[Time] aligned to rtc: system clock was %+lldms (set off an edge, not drift)\n",
                    (long long)(gain / 1000));
    }
    edgeSet_ = true;
    edgeAt_ = now;
    return INT64_MAX;
}
}
