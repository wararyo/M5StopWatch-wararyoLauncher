#pragma once
#include <cstdint>
#include <ctime>
namespace launcher {
// The host keeps its clock in UTC and converts only for display and manual
// entry (plan.md 7.3). There is no DST and no timezone UI, so a fixed offset is
// the entire conversion. Nothing here touches TZ, tzset or localtime_r: the
// prototype swapped the TZ environment variable around every call, which is
// global state the PC tests cannot pin down and the device pays for on each
// read. The arithmetic below is pure, so month ends, leap days and the JST date
// boundary are all checkable off-device.
constexpr int64_t JstOffsetSec=9*3600;
struct CivilTime { int year=0,month=0,day=0,hour=0,minute=0,second=0; };
constexpr bool leapYear(int y) { return y%4==0 && (y%100!=0 || y%400==0); }
constexpr int daysInMonth(int y,int m) {
    constexpr int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return m<1 || m>12 ? 0 : (m==2 && leapYear(y) ? 29 : lengths[m-1]);
}
constexpr bool validCivil(const CivilTime& c) {
    return c.month>=1 && c.month<=12 && c.day>=1 && c.day<=daysInMonth(c.year,c.month) &&
        c.hour>=0 && c.hour<24 && c.minute>=0 && c.minute<60 && c.second>=0 && c.second<60;
}
// Howard Hinnant's days_from_civil / civil_from_days. Day 0 is 1970-01-01 and
// the era arithmetic is exact for every year an RTC can hold.
constexpr int64_t daysFromCivil(int y,int m,int d) {
    y-=m<=2;
    const int64_t era=(y>=0 ? y : y-399)/400;
    const int yoe=int(y-era*400);
    const int doy=(153*(m+(m>2 ? -3 : 9))+2)/5+d-1;
    const int doe=yoe*365+yoe/4-yoe/100+doy;
    return era*146097+doe-719468;
}
constexpr void civilFromDays(int64_t z,int& y,int& m,int& d) {
    z+=719468;
    const int64_t era=(z>=0 ? z : z-146096)/146097;
    const int64_t doe=z-era*146097;
    const int64_t yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
    const int64_t doy=doe-(365*yoe+yoe/4-yoe/100);
    const int64_t mp=(5*doy+2)/153;
    d=int(doy-(153*mp+2)/5+1);
    m=int(mp+(mp<10 ? 3 : -9));
    y=int(yoe+era*400+(m<=2));
}
// Floor division on both: the launcher never sees a pre-1970 value, but a wild
// clock must not fold into the wrong day or a negative time of day.
constexpr int64_t dayFromUnix(int64_t seconds) {
    return seconds/86400-(seconds%86400<0 ? 1 : 0);
}
constexpr int64_t unixFromCivil(const CivilTime& c) {
    return daysFromCivil(c.year,c.month,c.day)*86400+c.hour*3600LL+c.minute*60LL+c.second;
}
constexpr CivilTime civilFromUnix(int64_t seconds) {
    const int64_t days=dayFromUnix(seconds);
    int64_t rest=seconds-days*86400;
    CivilTime c{};
    civilFromDays(days,c.year,c.month,c.day);
    c.hour=int(rest/3600); c.minute=int(rest/60%60); c.second=int(rest%60);
    return c;
}
// 1970-01-01 was a Thursday. 0 = Sunday, as std::tm wants.
constexpr int weekdayFromDays(int64_t days) { return int((days%7+11)%7); }
constexpr int yeardayFromCivil(const CivilTime& c) {
    return int(daysFromCivil(c.year,c.month,c.day)-daysFromCivil(c.year,1,1));
}
// std::tm appears only at the display boundary, because WatchData carries one.
inline void tmFromUnix(int64_t seconds,std::tm& out) {
    const CivilTime c=civilFromUnix(seconds);
    out=std::tm{};
    out.tm_year=c.year-1900; out.tm_mon=c.month-1; out.tm_mday=c.day;
    out.tm_hour=c.hour; out.tm_min=c.minute; out.tm_sec=c.second;
    out.tm_wday=weekdayFromDays(dayFromUnix(seconds));
    out.tm_yday=yeardayFromCivil(c);
    out.tm_isdst=0;
}
}
