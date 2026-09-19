#include "TimeService.h"
#include <M5Unified.h>
#include <cstdlib>
#include <sys/time.h>

namespace clock_service {

bool TimeService::begin() {
    const auto rtc = M5.Rtc.getDateTime();
    std::tm utc{};
    utc.tm_year = rtc.date.year - 1900;
    utc.tm_mon = rtc.date.month - 1;
    utc.tm_mday = rtc.date.date;
    utc.tm_hour = rtc.time.hours;
    utc.tm_min = rtc.time.minutes;
    utc.tm_sec = rtc.time.seconds;
    valid_ = rtc.date.year >= 2024 && rtc.date.year <= 2099 &&
             rtc.date.month >= 1 && rtc.date.month <= 12;
    if (!valid_) return false;

    setenv("TZ", "UTC0", 1);
    tzset();
    const time_t timestamp = mktime(&utc);
    const timeval tv{timestamp, 0};
    settimeofday(&tv, nullptr);
    setenv("TZ", "JST-9", 1);
    tzset();
    return true;
}

bool TimeService::now(std::tm& local) const {
    const time_t current = time(nullptr);
    return valid_ && localtime_r(&current, &local) != nullptr;
}

}  // namespace clock_service

