#include "host/HostApplication.h"
#include "features/home/HomeDataSource.h"
#include "services/TimeService.h"
#include "power/BatteryCurve.h"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
struct StubHal : Hal {
    CivilTime rtc{2026,9,20,15,0,0};
    bool readable=true,writable=true,acceptWrite=true;
    int writes=0,reads=0,clockSets=0,brightness=-1;
    int batteryReads=0,sleeps=0,wakes=0;
    int64_t clockUs=0;
    TimeUs time=0;
    InputSnapshot input{};
    BatteryState battery{};
    TimeUs now() override { return time; }
    InputSnapshot sampleInput() override { return input; }
    UsbState sampleUsb() override { return {}; }
    void setScreenOff(bool off) override { off ? ++sleeps : ++wakes; }
    void waitUs(TimeUs) override {}
    // A low-level interrupt: pending for as long as anything is pressed.
    bool inputPending() override { return input.a || input.b || input.touching; }
    bool readRtc(CivilTime& utc) override { ++reads; if(!readable) return false; utc=rtc; return true; }
    bool writeRtc(const CivilTime& utc) override {
        ++writes; if(!writable) return false; if(acceptWrite) rtc=utc; return true;
    }
    void setUtcClock(int64_t seconds) override { ++clockSets; clockUs=seconds*1000000; }
    int64_t utcClockUs() override { return clockUs; }
    BatteryState sampleBattery() override { ++batteryReads; return battery; }
    void setBrightness(int level) override { brightness=level; }
};
struct StubRender : RenderPort {
    int draws=0,invalidations=0;
    void invalidate() override { ++invalidations; }
    void draw(const FrameModel&,const WatchData&) override { ++draws; }
    TimeUs nextUpdate(TimeUs now,const WatchData& d) const override { return nextMinute(now,d); }
};
}
void calendar() {
    CHECK(leapYear(2024) && leapYear(2000) && leapYear(2028));
    CHECK(!leapYear(2026) && !leapYear(1900) && !leapYear(2100));
    CHECK(daysInMonth(2028,2)==29 && daysInMonth(2026,2)==28);
    CHECK(daysInMonth(2026,4)==30 && daysInMonth(2026,12)==31 && daysInMonth(2026,0)==0);
    CHECK(validCivil({2028,2,29,0,0,0}) && !validCivil({2026,2,29,0,0,0}));
    CHECK(!validCivil({2026,2,30,0,0,0}) && !validCivil({2026,13,1,0,0,0}));
    CHECK(!validCivil({2026,9,21,24,0,0}) && !validCivil({2026,9,21,0,60,0}));
    CHECK(unixFromCivil({1970,1,1,0,0,0})==0);
    CHECK(unixFromCivil({1970,1,2,0,0,0})==86400);
    CHECK(weekdayFromDays(0)==4);   // 1970-01-01 was a Thursday.
    CHECK(weekdayFromDays(-1)==3);  // Floor division must not skip a day.
    // Every day of the supported window round-trips and the weekday advances by
    // exactly one, which also pins the month ends and every leap day in range.
    int64_t day=daysFromCivil(TimeService::MinYear,1,1);
    const int64_t last=daysFromCivil(TimeService::MaxYear,12,31);
    int weekday=weekdayFromDays(day);
    int checked=0,leapDays=0;
    for (; day<=last; ++day) {
        const CivilTime c=civilFromUnix(day*86400);
        CHECK(validCivil(c) && daysFromCivil(c.year,c.month,c.day)==day);
        CHECK(weekdayFromDays(day)==weekday);
        if (c.month==2 && c.day==29) ++leapDays;
        weekday=(weekday+1)%7; ++checked;
    }
    CHECK(checked==27759 && leapDays==19);
    // Time of day survives the split, including the last second of a month.
    const CivilTime endOfMonth{2026,1,31,23,59,59};
    CHECK(civilFromUnix(unixFromCivil(endOfMonth)).day==31);
    CHECK(civilFromUnix(unixFromCivil(endOfMonth)+1).month==2);
}
void jstBoundary() {
    std::tm jst{};
    // 2026-09-20 15:00 UTC is exactly 2026-09-21 00:00 JST: a date boundary the
    // UTC day has not reached yet.
    tmFromUnix(unixFromCivil({2026,9,20,15,0,0})+JstOffsetSec,jst);
    CHECK(jst.tm_year==126 && jst.tm_mon==8 && jst.tm_mday==21);
    CHECK(jst.tm_hour==0 && jst.tm_min==0 && jst.tm_sec==0);
    CHECK(jst.tm_wday==1); // Monday.
    CHECK(jst.tm_yday==263 && jst.tm_isdst==0);
    tmFromUnix(unixFromCivil({2026,9,20,14,59,59})+JstOffsetSec,jst);
    CHECK(jst.tm_mday==20 && jst.tm_hour==23 && jst.tm_wday==0);
    tmFromUnix(unixFromCivil({2026,1,1,0,0,0}),jst);
    CHECK(jst.tm_yday==0 && jst.tm_mon==0);
    // A leap day in JST comes from the previous UTC day.
    tmFromUnix(unixFromCivil({2028,2,28,15,0,0})+JstOffsetSec,jst);
    CHECK(jst.tm_mon==1 && jst.tm_mday==29);
}
void startup() {
    StubHal hal; TimeService service;
    CHECK(service.begin(hal) && service.valid());
    std::tm jst{}; TimeUs sub=-1;
    CHECK(service.now(jst,sub) && sub==0);
    CHECK(jst.tm_mday==21 && jst.tm_hour==0 && jst.tm_min==0);
    hal.clockUs+=1500000;
    CHECK(service.now(jst,sub) && jst.tm_sec==1 && sub==500000);
    // An untrusted RTC never reaches the system clock, and the watch face is
    // told the time is unknown rather than shown the year 1900.
    for (const CivilTime bad : {CivilTime{1900,1,1,0,0,0},CivilTime{2023,12,31,23,59,59},
                                CivilTime{2100,1,1,0,0,0},CivilTime{2026,2,30,0,0,0}}) {
        StubHal invalid; invalid.rtc=bad; TimeService untrusted;
        CHECK(!untrusted.begin(invalid) && !untrusted.valid());
        CHECK(invalid.clockSets==0 && !untrusted.now(jst,sub));
    }
    StubHal unreadable; unreadable.readable=false; TimeService offline;
    CHECK(!offline.begin(unreadable) && !offline.valid());
}
void manualSave() {
    StubHal hal; TimeService service; service.begin(hal);
    const int writesBefore=hal.writes;
    CHECK(service.save({2026,2,30,12,0,0})==SaveResult::Invalid);
    CHECK(service.save({2023,6,1,12,0,0})==SaveResult::Invalid);
    CHECK(service.save({2026,6,1,24,0,0})==SaveResult::Invalid);
    CHECK(hal.writes==writesBefore && service.valid()); // Rejected before any write.
    // JST in, UTC on the wire, seconds zeroed.
    CHECK(service.save({2026,9,21,9,41,37})==SaveResult::Saved);
    CHECK(hal.rtc.year==2026 && hal.rtc.month==9 && hal.rtc.day==21);
    CHECK(hal.rtc.hour==0 && hal.rtc.minute==41 && hal.rtc.second==0);
    CHECK(hal.clockUs==unixFromCivil({2026,9,21,0,41,0})*1000000);
    std::tm jst{}; TimeUs sub=0;
    CHECK(service.now(jst,sub) && jst.tm_hour==9 && jst.tm_min==41 && jst.tm_sec==0);
    // A JST date that belongs to the previous UTC day still writes correctly.
    CHECK(service.save({2026,9,21,0,30,0})==SaveResult::Saved);
    CHECK(hal.rtc.day==20 && hal.rtc.hour==15 && hal.rtc.minute==30);
    CHECK(service.now(jst,sub) && jst.tm_mday==21 && jst.tm_hour==0);
    // The RTC ticking past the written second is the same write, not a mismatch.
    StubHal ticking; TimeService tick; tick.begin(ticking);
    ticking.acceptWrite=false;
    const int64_t target=unixFromCivil({2026,9,21,9,41,0})-JstOffsetSec;
    ticking.rtc=civilFromUnix(target+2);
    CHECK(tick.save({2026,9,21,9,41,0})==SaveResult::Saved && tick.valid());
    ticking.rtc=civilFromUnix(target+3); // Too far to be the write that just happened.
    CHECK(tick.save({2026,9,21,9,41,0})==SaveResult::RtcWriteFailed);
}
void saveFailure() {
    // The chip refuses the write: the old time survives and stays displayable.
    StubHal hal; TimeService service; service.begin(hal);
    hal.writable=false;
    const int64_t before=hal.clockUs;
    CHECK(service.save({2026,9,21,9,41,0})==SaveResult::RtcWriteFailed);
    CHECK(service.valid() && hal.clockUs==before);
    // The write is acknowledged but nothing sticks; the read-back catches it.
    StubHal silent; TimeService quiet; quiet.begin(silent);
    silent.acceptWrite=false;
    CHECK(quiet.save({2026,9,21,9,41,0})==SaveResult::RtcWriteFailed);
    CHECK(quiet.valid()); // Recovered from the value the RTC still holds.
    // Recovery itself fails: say so instead of showing a time nothing backs.
    StubHal broken; TimeService lost; lost.begin(broken);
    broken.acceptWrite=false; broken.readable=false;
    CHECK(lost.save({2026,9,21,9,41,0})==SaveResult::Unrecoverable);
    CHECK(!lost.valid());
    std::tm jst{}; TimeUs sub=0;
    CHECK(!lost.now(jst,sub));
    // A write that lands on a value outside the trusted window is not a save.
    StubHal drifting; TimeService drift; drift.begin(drifting);
    drifting.acceptWrite=false; drifting.rtc={2100,1,1,0,0,0};
    CHECK(drift.save({2026,9,21,9,41,0})==SaveResult::Unrecoverable && !drift.valid());
}
void displayData() {
    StubHal hal; TimeService service; service.begin(hal);
    HomeDataSource data(hal,service);
    const auto first=data.sample(1000);
    CHECK(hal.batteryReads==1 && first.batteryPercent==-1 && !first.charging);
    CHECK(data.nextUpdate(1000)==1000+BatteryPeriodUs);
    hal.battery={82,true};
    data.sample(1000+BatteryPeriodUs-1);
    CHECK(hal.batteryReads==1); // Still cached; the face keeps the old percent.
    const auto refreshed=data.sample(1000+BatteryPeriodUs);
    CHECK(hal.batteryReads==2 && refreshed.batteryPercent==82 && refreshed.charging);
    CHECK(data.nextUpdate(0)==1000+2*BatteryPeriodUs);
    // The clock comes from the service in JST, with the real sub-second, so the
    // watch face can put the next redraw on the minute boundary.
    hal.clockUs=unixFromCivil({2026,9,20,15,0,0})*1000000+250000;
    const auto stamped=data.sample(1000+BatteryPeriodUs);
    CHECK(hal.batteryReads==2 && stamped.timeValid);
    CHECK(stamped.localTime.tm_mday==21 && stamped.localTime.tm_hour==0 && stamped.localTime.tm_wday==1);
    CHECK(stamped.subsecondUs==250000);
    CHECK(nextMinute(0,stamped)==60*1000000LL-250000);
    // An untrusted RTC leaves the time unknown but still reports the battery.
    StubHal dead; dead.rtc={1900,1,1,0,0,0}; dead.battery={55,false};
    TimeService none; none.begin(dead);
    HomeDataSource unset(dead,none);
    const auto blank=unset.sample(0);
    CHECK(!blank.timeValid && blank.batteryPercent==55 && blank.subsecondUs==0);
}
void batteryCurve() {
    // Points read back exactly, the ends clamp, and between points it rounds.
    CHECK(batteryPercentFromMv(4300)==100 && batteryPercentFromMv(4200)==100);
    CHECK(batteryPercentFromMv(3700)==52 && batteryPercentFromMv(3600)==29);
    CHECK(batteryPercentFromMv(3100)==0 && batteryPercentFromMv(2900)==0);
    CHECK(batteryPercentFromMv(3675)==47); // 41 + 11*25/50 = 46.5
    CHECK(batteryPercentFromMv(3250)==2);
    // Never rises as the voltage falls.
    int previous=100;
    for(int mv=4300;mv>=2900;--mv) { const int p=batteryPercentFromMv(mv); CHECK(p<=previous && p>=0); previous=p; }
}
void batteryEstimator() {
    const ChargeInput battery{false,true,false,false}, charging{true,true,true,false}, full{true,true,false,false};
    const auto mv=[](int value) { return [value] { return value; }; };
    BatteryEstimator e;
    // Each reading stands on its own: noise shows as it is.
    CHECK(e.update(battery,mv(3700))==52);
    CHECK(e.update(battery,mv(3710))==53);
    CHECK(e.update(battery,mv(0))==-1); // A failed read is unknown.
    // While charging the voltage less the charger's 44 mV lift: 3744 mV
    // charging reads as 3700 mV rested, on the same curve.
    CHECK(e.update(charging,mv(3744))==52);
    CHECK(e.update(charging,mv(4212))==99);
    CHECK(e.update(charging,mv(4300))==99); // Only the charger says 100.
    CHECK(e.update(full,mv(4200))==100);
    // Settling after charging stops: the last value stays, and the voltage
    // is not even read.
    int reads=0;
    const auto counted=[&reads] { ++reads; return 3600; };
    const ChargeInput settling{false,true,false,true};
    CHECK(e.update(settling,counted)==100 && reads==0);
    CHECK(e.update(battery,counted)==29 && reads==1);
    // With nothing to hold yet, it reads.
    BatteryEstimator fresh;
    CHECK(fresh.update(settling,mv(3700))==52);
    // A failed status read on battery is not charging.
    CHECK(e.update({false,true,true,false},mv(4100))==93);
    // On USB with the charger idle well below full: paused, not full.
    CHECK(e.update(full,mv(3900))==74);
}
void alignment() {
    // The system clock is set at the RTC's second edge, read once per call.
    StubHal hal; TimeService service; service.begin(hal);
    const int64_t second=unixFromCivil(hal.rtc);
    hal.clockUs=second*1000000+700000; // Gained 0.7s since the last edge.
    bool stepped=true;
    CHECK(service.align(0,stepped)==INT64_MAX && !stepped); // Not begun.
    const int reads=hal.reads, sets=hal.clockSets;
    service.beginAlign(0);
    CHECK(service.align(0,stepped)==TimeService::AlignPollUs && !stepped);
    CHECK(service.align(5000,stepped)==5000+TimeService::AlignPollUs && !stepped);
    hal.rtc.second=1;
    CHECK(service.align(10000,stepped)==INT64_MAX && stepped);
    CHECK(hal.clockUs==(second+1)*1000000 && hal.clockSets==sets+1 && hal.reads==reads+3);
    CHECK(service.align(15000,stepped)==INT64_MAX && !stepped && hal.reads==reads+3); // Done.
    // No edge within the window: the clock is left alone.
    service.beginAlign(20000); service.align(20000,stepped);
    CHECK(service.align(20000+TimeService::AlignWindowUs+1,stepped)==INT64_MAX && !stepped);
    CHECK(hal.clockSets==sets+1);
    // Neither is it on a failed read.
    service.beginAlign(0); hal.readable=false;
    CHECK(service.align(0,stepped)==INT64_MAX && !stepped && hal.clockSets==sets+1);
    // A late read does not take an edge, which may lie that far back; the
    // next edge read in time does.
    hal.readable=true;
    const TimeUs start=30000000, late=start+TimeService::AlignGapUs+1;
    service.beginAlign(start); service.align(start,stepped);
    hal.rtc.second=2;
    CHECK(service.align(late,stepped)==late+TimeService::AlignPollUs && !stepped && hal.clockSets==sets+1);
    hal.rtc.second=3;
    CHECK(service.align(late+TimeService::AlignPollUs,stepped)==INT64_MAX && stepped);
    CHECK(hal.clockUs==(second+3)*1000000 && hal.clockSets==sets+2);
}
void runtimeIntegration() {
    StubHal hal; StubRender render; TimeService service; service.begin(hal);
    HomeDataSource data(hal,service);
    HostApplication application(hal,render,data,468,468); auto& runtime=application.runtime();
    runtime.begin(); runtime.step();
    CHECK(render.draws==1 && hal.batteryReads==1);
    // No input for the sleep timeout: the panel goes dark before the battery
    // period expires, so the read never happens behind a black screen.
    for(int i=0;i<31;++i) { hal.time+=1000000; runtime.step(); }
    CHECK(hal.sleeps==1 && hal.batteryReads==1);
    const int draws=render.draws;
    for(int i=0;i<300;++i) { hal.time+=1000000; runtime.step(); }
    CHECK(render.draws==draws && hal.batteryReads==1); // Nothing while dark.
    // Waking redraws once, and that frame carries a fresh battery reading.
    hal.battery={64,false};
    hal.input={false,false,true,100,100}; hal.time+=10000; runtime.step();
    CHECK(hal.wakes==1 && render.draws==draws+1 && hal.batteryReads==2);
    // The wake aligns the clock with the RTC, polled between frames.
    const int reads=hal.reads;
    hal.input={}; hal.time+=TimeService::AlignPollUs; runtime.step();
    CHECK(hal.reads==reads+1);
    hal.rtc.second=(hal.rtc.second+1)%60; hal.time+=TimeService::AlignPollUs; runtime.step();
    const int aligned=hal.reads;
    hal.time+=TimeService::AlignPollUs; runtime.step(); CHECK(hal.reads==aligned);
}
int main() {
    calendar(); jstBoundary(); startup(); manualSave(); saveFailure();
    displayData(); batteryCurve(); batteryEstimator(); alignment(); runtimeIntegration();
    std::cout << "PASS: calendar, jst boundary, startup, manual save, save failure, "
                 "display data, battery curve, battery estimator, rtc alignment, runtime integration\n";
}
