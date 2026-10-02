#include "TimeService.h"
#include <cstdio>
namespace launcher {
bool TimeService::syncFromRtc() {
    CivilTime utc{};
    if (!hal_ || !hal_->readRtc(utc) || !trusted(utc)) { valid_ = false; return false; }
    hal_->setUtcClock(unixFromCivil(utc));
    valid_ = true;
    return true;
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
    hal_->setUtcClock(unixFromCivil(readback));
    valid_ = true;
    return SaveResult::Saved;
}
void TimeService::beginAlign(TimeUs now) {
    if (!hal_) return;
    alignUntil_ = now + AlignWindowUs;
    alignFirst_ = true;
}
TimeUs TimeService::align(TimeUs now, bool& stepped) {
    stepped = false;
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
    if (alignFirst_ || second == alignSecond_) {
        alignFirst_ = false;
        alignSecond_ = second;
        return now + AlignPollUs;
    }
    // The edge came since the previous read, at most one poll ago.
    const int64_t before = hal_->utcClockUs();
    hal_->setUtcClock(second);
    valid_ = true;
    alignUntil_ = INT64_MIN;
    stepped = true;
    std::printf("[Time] aligned to rtc: system clock was %+lldms\n",
                (long long)((before - second * 1000000) / 1000));
    return INT64_MAX;
}
}
