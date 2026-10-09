#include "host/HostApplication.h"
#include "host/LaunchRegistry.h"
#include "features/pedometer/PedometerLayout.h"
#include "i18n/Strings.h"
#include "multifirm/FakeSlotService.h"
#include "services/TimeService.h"
#include "storage/SettingsStore.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
constexpr TimeUs Second=1000000,Minute=60*Second,Hour=60*Minute;
// UTC microseconds of a JST wall-clock time.
int64_t jstUs(int y,int mo,int d,int h,int mi,int s) {
    return (unixFromCivil(CivilTime{y,mo,d,h,mi,s})-JstOffsetSec)*1000000;
}
int64_t dayOf(int y,int mo,int d) { return daysFromCivil(y,mo,d); }
// An IMU that counts what the test says, a crystal RTC, and a system clock
// that is the monotonic one plus an offset, as on the device
// (LIBC_TIME_SYSCALL_USE_RTC_HRT). While light sleep is allowed the monotonic
// clock gains `ppm`; an alignment sets the system clock to the RTC at the
// poll after it begins, as TimeService does.
struct PedometerHal : Hal, RenderPort, DisplayDataSource {
    TimeUs time=0,gained=0;
    TimeUs ppm=0;
    int64_t rtcBase=0;   // UTC us the RTC reads at real time 0.
    int64_t offset=0;    // System clock minus monotonic clock.
    bool rtcSet=true,lightSleep=false,events=true;
    UsbState usb{true,0,false};   // On battery, so the dark panel may sleep.
    InputSnapshot input{};
    uint32_t steps=0;
    bool imu=true;
    int reads=0,draws=0,wakes=0,alignments=0;
    TimeUs edgeAt=INT64_MAX;
    TimeUs drawEvery=INT64_MAX;  // The clock's own deadline after a draw.
    TimeUs real() const { return time-gained; }
    TimeUs now() override { return time; }
    bool readRtc(CivilTime& utc) override {
        if (!rtcSet) return false;
        const int64_t us=rtcBase+real();
        utc=civilFromUnix(us/1000000);
        return true;
    }
    bool writeRtc(const CivilTime& utc) override {
        rtcBase=unixFromCivil(utc)*1000000-real(); rtcSet=true; return true;
    }
    void setUtcClock(int64_t unixSeconds) override { offset=unixSeconds*1000000-time; }
    int64_t utcClockUs() override { return time+offset; }
    BatteryState sampleBattery() override { return {}; }
    void setBrightness(int) override {}
    InputSnapshot sampleInput() override { return input; }
    UsbState sampleUsb() override { return usb; }
    bool usbEvents() const override { return events; }
    void setLightSleepAllowed(bool allowed) override { lightSleep=allowed; }
    void setScreenOff(bool off) override { if (!off) ++wakes; }
    bool inputPending() override { return input.a || input.b || input.touching; }
    void waitUs(TimeUs delay) override {
        time+=delay;
        if (lightSleep) gained+=delay*ppm/(1000000+ppm);
    }
    bool readStepCount(uint32_t& count) override { ++reads; if (!imu) return false; count=steps; return true; }
    void invalidate() override {}
    void draw(const FrameModel&,const WatchData&) override { ++draws; }
    TimeUs nextUpdate(TimeUs now,const WatchData&) const override {
        return drawEvery==INT64_MAX ? INT64_MAX : now+drawEvery;
    }
    void alignClock(TimeUs now) override { ++alignments; edgeAt=now+TimeService::AlignPollUs; }
    TimeUs service(TimeUs now,ClockAlignment& a) override {
        a={};
        if (edgeAt==INT64_MAX) return INT64_MAX;
        if (now<edgeAt) return edgeAt;
        a.stepped=true;
        offset=rtcBase+real()-time;
        edgeAt=INT64_MAX;
        return INT64_MAX;
    }
};
struct MemoryRecords : RecordBackend {
    std::map<std::string,std::vector<uint8_t>> records;
    bool writable=true;
    int writes=0;
    PrefResult read(const char* key,uint8_t* data,size_t& size) override {
        const auto found=records.find(key);
        if (found==records.end()) return PrefResult::Missing;
        if (found->second.size()>size) return PrefResult::Invalid;
        size=found->second.size();
        std::copy(found->second.begin(),found->second.end(),data);
        return PrefResult::Ok;
    }
    PrefResult write(const char* key,const uint8_t* data,size_t size) override {
        ++writes;
        if (!writable) return PrefResult::WriteFailed;
        records[key].assign(data,data+size);
        return PrefResult::Ok;
    }
    void put(int32_t day,uint32_t steps) {
        uint8_t record[PedometerRecord::RecordBytes];
        records[PedometerRecord::Key].assign(record,record+PedometerRecord::encode(day,steps,record));
    }
    bool saved(int32_t& day,uint32_t& steps) const {
        const auto found=records.find(PedometerRecord::Key);
        return found!=records.end() &&
            PedometerRecord::decode(found->second.data(),found->second.size(),day,steps);
    }
};
// The application on the PC, as main wires it: clock, settings, records.
struct Rig {
    PedometerHal hal;
    MemoryRecords records;
    TimeService time;
    SettingsStore settings;
    HostApplication app{hal,hal,hal,468,468};
    HostRuntime& runtime=app.runtime();
    PedometerService& pedometer=app.pedometer();
    // Starts with the RTC at `utcUs` (or unset) and the given IMU count.
    explicit Rig(int64_t utcUs,bool clockSet=true,const std::vector<std::pair<int32_t,uint32_t>>& record={}) {
        hal.rtcBase=utcUs; hal.rtcSet=clockSet;
        for (auto& r:record) records.put(r.first,r.second);
        time.begin(hal);
        app.bindSettings(settings,time);
        app.bindRecords(records);
        runtime.begin(); runtime.step();
    }
    void at(TimeUs t) { hal.time=t; runtime.step(); }
    void follow(TimeUs from,TimeUs to) { for (TimeUs t=from;t<=to;t+=10000) at(t); }
    void press(bool a,TimeUs held=20000) {
        (a ? hal.input.a : hal.input.b)=true; follow(hal.time+10000,hal.time+held);
        (a ? hal.input.a : hal.input.b)=false; at(hal.time+10000);
        at(hal.time+200000);
    }
    // Lets the loop run on its own deadlines until `until` (monotonic).
    void run(TimeUs until) {
        while (hal.time<until) { runtime.wait(); runtime.step(); }
    }
    // Until the panel goes dark by its timeout.
    void darken() { while (!runtime.power().screenOff()) at(hal.time+Second); }
    int64_t jstNow() { return hal.utcClockUs()+JstOffsetSec*1000000; }
};

void numbersDays() {
    // A day runs 04:00 to 04:00 JST and is named for the date it starts on.
    CHECK(pedometerDay(jstUs(2026,10,7,3,59,59)/1000000)==dayOf(2026,10,6));
    CHECK(pedometerDay(jstUs(2026,10,7,4,0,0)/1000000)==dayOf(2026,10,7));
    CHECK(pedometerDay(jstUs(2026,10,7,23,59,59)/1000000)==dayOf(2026,10,7));
    CHECK(pedometerDay(jstUs(2026,10,8,0,0,0)/1000000)==dayOf(2026,10,7));
    CHECK(pedometerDayStart(dayOf(2026,10,7))*1000000==jstUs(2026,10,7,4,0,0));
    // Month and year ends, and a leap day.
    CHECK(pedometerDay(jstUs(2026,11,1,3,0,0)/1000000)==dayOf(2026,10,31));
    CHECK(pedometerDay(jstUs(2027,1,1,3,0,0)/1000000)==dayOf(2026,12,31));
    CHECK(pedometerDay(jstUs(2028,3,1,3,0,0)/1000000)==dayOf(2028,2,29));
    CHECK(pedometerDayStart(dayOf(2028,2,29)+1)*1000000==jstUs(2028,3,1,4,0,0));
}
void plansAlignments() {
    CHECK(pedometerAlignmentLeft(24*Hour)==10*Minute);
    CHECK(pedometerAlignmentLeft(10*Minute+1)==10*Minute);
    CHECK(pedometerAlignmentLeft(10*Minute)==Minute);
    CHECK(pedometerAlignmentLeft(Minute+1)==Minute);
    CHECK(pedometerAlignmentLeft(Minute)==0);
}
void keepsTheRecord() {
    uint8_t bytes[PedometerRecord::RecordBytes];
    CHECK(PedometerRecord::encode(20733,12345,bytes)==9 && bytes[0]==1);
    int32_t day=0; uint32_t steps=0;
    CHECK(PedometerRecord::decode(bytes,9,day,steps) && day==20733 && steps==12345);
    CHECK(!PedometerRecord::decode(bytes,8,day,steps));
    bytes[0]=2; CHECK(!PedometerRecord::decode(bytes,9,day,steps));   // Unknown format.
    MemoryRecords records;
    PedometerRecord record; record.bind(&records);
    CHECK(record.load()==PrefResult::Missing && !record.has());
    // Written once; the same day and steps again cost nothing.
    CHECK(record.remember(10,5)==PrefResult::Ok && records.writes==1);
    CHECK(record.remember(10,5)==PrefResult::Ok && records.writes==1);
    // A failure keeps comparing with what last landed.
    records.writable=false;
    CHECK(record.remember(10,6)==PrefResult::WriteFailed && records.writes==2);
    CHECK(record.day()==10 && record.steps()==5);
    records.writable=true;
    CHECK(record.remember(10,6)==PrefResult::Ok && records.writes==3);
    // An unusable record is reported and left alone.
    records.records[PedometerRecord::Key]={2,0,0,0,0,0,0,0,0};
    CHECK(record.load()==PrefResult::Invalid && !record.has());
    // The key is the host's: no face may take it.
    CHECK(reservedRecordKey("pedometer"));
}
void counts() {
    Rig r(jstUs(2026,10,7,12,0,0));
    auto& p=r.pedometer;
    CHECK(p.available() && p.dated() && p.day()==dayOf(2026,10,7) && p.today()==0);
    r.hal.steps=100; CHECK(p.refresh(r.hal.time) && p.today()==100);
    // The IMU began again (it counts from 0): what it had is still today's.
    r.hal.steps=30; CHECK(p.refresh(r.hal.time) && p.today()==130);
    r.hal.steps=50; CHECK(p.refresh(r.hal.time) && p.today()==150);
    // A read that fails keeps the count.
    r.hal.imu=false; CHECK(!p.refresh(r.hal.time) && p.today()==150);
    // Without an IMU there is nothing to count.
    PedometerHal bare; bare.imu=false;
    PedometerService none(bare);
    none.begin(0,0,0,false);
    CHECK(!none.available() && none.today()==0);
}
void carriesOverRestarts() {
    const int32_t today=int32_t(dayOf(2026,10,7));
    {   // A record of today carries on.
        Rig r(jstUs(2026,10,7,12,0,0),true,{{today,500}});
        CHECK(r.pedometer.today()==500);
        r.hal.steps=20; r.pedometer.refresh(r.hal.time);
        CHECK(r.pedometer.today()==520);
    }
    {   // Yesterday's does not, and is overwritten at the next save.
        Rig r(jstUs(2026,10,7,12,0,0),true,{{today-1,500}});
        CHECK(r.pedometer.today()==0);
        r.hal.steps=8; r.darken();
        int32_t day=0; uint32_t steps=0;
        CHECK(r.records.saved(day,steps) && day==today && steps==8);
    }
    {   // With the clock unset there is no day: nothing carries on, nothing
        // is saved.
        Rig r(0,false,{{today,500}});
        CHECK(!r.pedometer.dated() && r.pedometer.today()==0);
        r.hal.steps=40; r.darken();
        CHECK(r.records.writes==0);
        int32_t day=0; uint32_t steps=0;
        CHECK(r.records.saved(day,steps) && steps==500);
    }
}
void savesGoingDark() {
    Rig r(jstUs(2026,10,7,12,0,0));
    const int32_t today=int32_t(dayOf(2026,10,7));
    int32_t day=0; uint32_t steps=0;
    r.hal.steps=50; r.darken();
    CHECK(r.records.writes==1 && r.records.saved(day,steps) && day==today && steps==50);
    // Woken and dark again with nothing new: no write.
    r.press(true); CHECK(!r.runtime.power().screenOff());
    r.darken(); CHECK(r.records.writes==1);
    r.press(true); r.hal.steps=60; r.darken();
    CHECK(r.records.writes==2 && r.records.saved(day,steps) && steps==60);
    // A failed write is tried again at the next dark, against what landed.
    r.records.writable=false;
    r.press(true); r.hal.steps=70; r.darken();
    CHECK(r.records.writes==3 && r.records.saved(day,steps) && steps==60);
    r.records.writable=true;
    r.press(true); r.darken();
    CHECK(r.records.writes==4 && r.records.saved(day,steps) && steps==70);
}
void endsTheDayInTheDark() {
    // The monotonic clock gains 7,500ppm asleep, as seen on 2026-10-06: an
    // hour of it would end the day 27s early without the alignments.
    Rig r(jstUs(2026,10,7,3,0,0));
    r.hal.ppm=7500;
    auto& p=r.pedometer;
    const int64_t before=p.day();
    CHECK(before==dayOf(2026,10,6));
    r.darken(); CHECK(r.hal.lightSleep);
    r.hal.steps=1000;
    const int wakes=r.hal.wakes,alignments=r.hal.alignments,draws=r.hal.draws;
    while (p.day()==before && r.hal.time<2*Hour) { r.runtime.wait(); r.runtime.step(); }
    CHECK(p.day()==dayOf(2026,10,7));
    // Ended at 04:00 by the crystal, within the last minute's gain.
    const int64_t realJst=r.hal.rtcBase+r.hal.real();
    CHECK(realJst>=jstUs(2026,10,7,4,0,0)-Second && realJst<=jstUs(2026,10,7,4,0,0)+Second);
    // Aligned on the way (ten minutes and one minute before), the panel left
    // dark and nothing drawn.
    CHECK(r.hal.alignments-alignments==2);
    CHECK(r.hal.wakes==wakes && r.hal.draws==draws && r.runtime.power().screenOff());
    // The 1000 steps were yesterday's.
    CHECK(p.today()==0);
    r.hal.steps=1200; p.refresh(r.hal.time);
    CHECK(p.today()==200);
}
void retriesTheRead() {
    for (bool recovers:{false,true}) {
        Rig r(jstUs(2026,10,7,3,59,0));
        auto& p=r.pedometer;
        r.darken();
        const TimeUs end=r.hal.time+(jstUs(2026,10,7,4,0,0)-r.hal.utcClockUs());
        r.hal.steps=300;
        p.refresh(r.hal.time);
        r.hal.imu=false; r.hal.steps=310;
        while (r.hal.time<end) { r.runtime.wait(); r.runtime.step(); }
        CHECK(r.hal.time==end && p.day()==dayOf(2026,10,6));
        // A second later it tries again.
        r.runtime.wait(); CHECK(r.hal.time==end+Second);
        if (recovers) r.hal.imu=true;
        r.runtime.step();
        if (recovers) {
            // Read at last: the 10 steps since are the old day's.
            CHECK(p.day()==dayOf(2026,10,7) && p.today()==0);
            r.hal.steps=315; p.refresh(r.hal.time); CHECK(p.today()==5);
            continue;
        }
        CHECK(p.day()==dayOf(2026,10,6));
        r.runtime.wait(); CHECK(r.hal.time==end+2*Second);
        r.runtime.step();
        // The third failure ends the day on the last count read.
        CHECK(p.day()==dayOf(2026,10,7) && p.today()==0);
        r.hal.imu=true; p.refresh(r.hal.time); CHECK(p.today()==10);
    }
}
void followsTheClock() {
    {   // Set by hand to another day, either way: a new day from that moment.
        Rig r(jstUs(2026,10,7,12,0,0));
        auto& p=r.pedometer;
        r.hal.steps=100; p.refresh(r.hal.time);
        CHECK(r.time.save(CivilTime{2026,10,8,12,0,0})==SaveResult::Saved);
        r.at(r.hal.time+Second);
        CHECK(p.day()==dayOf(2026,10,8) && p.today()==0);
        r.hal.steps=130; p.refresh(r.hal.time); CHECK(p.today()==30);
        CHECK(r.time.save(CivilTime{2026,10,7,12,0,0})==SaveResult::Saved);
        r.at(r.hal.time+Second);
        CHECK(p.day()==dayOf(2026,10,7) && p.today()==0);
        // Within the same day nothing ends.
        CHECK(r.time.save(CivilTime{2026,10,7,20,0,0})==SaveResult::Saved);
        r.hal.steps=140; r.at(r.hal.time+Second);
        p.refresh(r.hal.time); CHECK(p.day()==dayOf(2026,10,7) && p.today()==10);
    }
    {   // Unset, then set: what was counted becomes the set day's.
        Rig r(0,false);
        auto& p=r.pedometer;
        r.hal.steps=40; p.refresh(r.hal.time);
        r.at(r.hal.time+Hour);
        CHECK(!p.dated() && p.today()==40);
        CHECK(r.time.save(CivilTime{2026,10,7,12,0,0})==SaveResult::Saved);
        r.at(r.hal.time+Second);
        CHECK(p.dated() && p.day()==dayOf(2026,10,7) && p.today()==40);
    }
}
void readsBesideTheClock() {
    // Each frame of a visible clock may read, but only once the count is 30s
    // old; nothing wakes for it.
    Rig r(jstUs(2026,10,7,12,0,0));
    r.hal.drawEvery=Second;
    const TimeUs begun=r.pedometer.lastReadAt();
    const int reads=r.hal.reads;
    r.run(r.hal.time+25*Second);
    CHECK(r.hal.draws>20 && r.hal.reads==reads && r.pedometer.lastReadAt()==begun);
    // The routine on its own: due at 30s, not before.
    PedometerRoutine& routine=r.app.pedometerRoutine();
    routine.beforeClock(begun+PedometerRoutine::ClockReadUs-1); CHECK(r.hal.reads==reads);
    routine.beforeClock(begun+PedometerRoutine::ClockReadUs); CHECK(r.hal.reads==reads+1);
    // Dark, it is read for nothing but the day's end (and the dark's save).
    r.darken();
    const int dark=r.hal.reads;
    r.run(r.hal.time+3*Hour);
    CHECK(r.hal.reads==dark);
}
void savesForAnExternalBoot() {
    Rig r(jstUs(2026,10,7,12,0,0));
    FakeSlotService slots; slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    slots.bootSucceeds=true;
    r.app.bindSlots(slots);
    r.hal.steps=77;
    r.press(true);
    while (LaunchRegistry[r.runtime.model().launcher.list.selection].id!=LaunchTargetId::External1) r.press(true);
    r.press(false);
    CHECK(slots.bootRequests==1);
    int32_t day=0; uint32_t steps=0;
    CHECK(r.records.saved(day,steps) && day==int32_t(dayOf(2026,10,7)) && steps==77);
}
std::string shown(const PedometerModel& m) { char out[16]; formatSteps(m,out,sizeof(out)); return out; }
void formatsTheCount() {
    CHECK(shown({true,0})=="0" && shown({true,999})=="999" && shown({true,1000})=="1,000");
    CHECK(shown({true,12345})=="12,345" && shown({true,123456})=="123,456");
    CHECK(shown({true,1234567})=="1,234,567" && shown({true,4294967295u})=="4,294,967,295");
    CHECK(shown({false,12345})=="--");
    // Cut short rather than overrun.
    char small[4]; formatSteps({true,12345},small,sizeof(small)); CHECK(std::string(small).size()<sizeof(small));
}
void sitsAfterTheTimer() {
    int timer=-1,pedometer=-1;
    for (int i=0;i<int(LaunchRegistry.size());++i) {
        if (LaunchRegistry[i].id==LaunchTargetId::Timer) timer=i;
        if (LaunchRegistry[i].id==LaunchTargetId::Pedometer) pedometer=i;
    }
    CHECK(timer>=0 && pedometer==timer+1);
    CHECK(int(LaunchTargetId::Pedometer)==6 && LaunchRegistry[pedometer].icon==IconId::Pedometer);
    CHECK(LaunchRegistry[pedometer].kind==TargetKind::Builtin && LaunchRegistry[pedometer].name==text::Pedometer);
    CHECK(appColors(LaunchRegistry[pedometer]).background==0x2d87 &&
          appColors(LaunchRegistry[pedometer]).foreground==AppIconWhite);
    // OK, and where it takes touches, stay inside the panel's circle.
    const Viewport m{468,468};
    for (const Rect& r:{pedometerOkBox(m),pedometerOkHitBox(m),pedometerCountBox(m),pedometerTitleBox(m)})
        for (int cx:{r.x,r.x+r.w}) for (int cy:{r.y,r.y+r.h}) {
            const int dx=cx-234,dy=cy-234;
            CHECK(dx*dx+dy*dy<=234*234);
        }
}
// From the clock: A into the list, along to the pedometer, B to open it.
void openPedometer(Rig& r) {
    r.press(true);
    while (LaunchRegistry[r.runtime.model().launcher.list.selection].id!=LaunchTargetId::Pedometer) r.press(true);
    r.press(false);
}
void tap(Rig& r,int x,int y) {
    r.hal.input.touching=true; r.hal.input.x=x; r.hal.input.y=y;
    r.follow(r.hal.time+10000,r.hal.time+50000);
    r.hal.input.touching=false; r.at(r.hal.time+10000); r.at(r.hal.time+200000);
}
void showsTheCount() {
    Rig r(jstUs(2026,10,7,12,0,0));
    r.hal.steps=12345;
    openPedometer(r);
    auto model=[&] { return r.runtime.model(); };
    CHECK(model().screen==ScreenId::Pedometer);
    // Read when it opened.
    CHECK(model().pedometer.available && model().pedometer.steps==12345);
    // Then every second while shown: a step is drawn within the second.
    r.hal.steps=12350;
    const int draws=r.hal.draws;
    r.run(r.hal.time+1100000);
    CHECK(model().pedometer.steps==12350 && r.hal.draws>draws);
    // A, B and a tap on OK go back to the list; a tap elsewhere does nothing.
    r.press(true); CHECK(model().screen==ScreenId::AppList);
    // Closed, it keeps no deadline of its own.
    CHECK(r.app.screens().nextUpdate()==INT64_MAX || r.app.screens().nextUpdate()>r.hal.time+Second);
    r.press(false); CHECK(model().screen==ScreenId::Pedometer);
    r.press(false); CHECK(model().screen==ScreenId::AppList);
    r.press(false);
    const Rect title=pedometerTitleBox({468,468});
    tap(r,title.x+title.w/2,title.y+title.h/2); CHECK(model().screen==ScreenId::Pedometer);
    const Rect ok=pedometerOkBox({468,468});
    tap(r,ok.x+ok.w/2,ok.y+ok.h+ok.h/2); CHECK(model().screen==ScreenId::AppList);
    // A+B goes home, as everywhere.
    r.press(false); CHECK(model().screen==ScreenId::Pedometer);
    r.hal.input.a=r.hal.input.b=true; r.follow(r.hal.time+10000,r.hal.time+700000);
    r.hal.input.a=r.hal.input.b=false; r.at(r.hal.time+10000);
    CHECK(model().screen==ScreenId::Home);
}
void opensWithoutAnImu() {
    PedometerHal hal; hal.imu=false;
    // The entry opens anyway, to say there is nothing to count.
    PedometerScreen screen; PedometerService service(hal); screen.bind(&service);
    CHECK(screen.available());
    screen.enter(0);
    CHECK(!screen.model().available && shown(screen.model())=="--");
    CHECK(screen.nextUpdate()==Second);
    screen.exit(); CHECK(screen.nextUpdate()==INT64_MAX);
    PedometerScreen unbound; CHECK(!unbound.available());
}
std::string line(uint32_t steps) { char out[BackgroundLabelBytes]; formatPedometerBackground(steps,out,sizeof(out)); return out; }
void labelsTheFace() {
    // Rounded down: tenths of a thousand, then whole thousands.
    CHECK(line(10000)=="10.0K" && line(10099)=="10.0K" && line(10100)=="10.1K" && line(10999)=="10.9K");
    CHECK(line(99999)=="99.9K" && line(100000)=="100K" && line(123456)=="123K" && line(4294967295u)=="4294967K");
    // Shown from 10,000 steps, with the pedometer's own look and no deadline.
    PedometerHal hal;
    PedometerService service(hal); service.begin(0,0,0,false);
    PedometerBackgroundInfo info(service);
    CHECK(info.id()==LaunchTargetId::Pedometer);
    BackgroundInfo out;
    hal.steps=9999; service.refresh(0); CHECK(!info.sample(0,out));
    hal.steps=10000; service.refresh(0); out=BackgroundInfo{};
    CHECK(info.sample(0,out) && std::string(out.label)=="10.0K" && out.nextChangeAt==INT64_MAX);
    CHECK(out.icon==appIcon(IconId::Pedometer) && out.suggestedColor==PedometerColors.background);
    // Without an IMU, nothing, whatever a record carried over.
    PedometerHal bare; bare.imu=false;
    PedometerService none(bare); none.begin(0,0,0,false);
    PedometerBackgroundInfo noInfo(none); out=BackgroundInfo{};
    CHECK(!noInfo.sample(0,out));
}
void reachesTheFaceLast() {
    Rig r(jstUs(2026,10,7,12,0,0));
    r.hal.drawEvery=Second;
    auto& hub=r.app.background();
    CHECK(hub.providers()==3);
    // Below 10,000 the clock shows nothing of it.
    r.hal.steps=9990; r.pedometer.refresh(r.hal.time);
    r.run(r.hal.time+2*Second);
    CHECK(hub.snapshot().count==0);
    // Read on a clock frame once the count is 30s old: then on the face,
    // after a running timer's line.
    CHECK(r.app.timer().start(r.hal.time,600));
    r.hal.steps=10050;
    r.run(r.hal.time+20*Second);
    tap(r,234,234);   // Keeps the panel lit past its 30s.
    CHECK(!r.runtime.power().screenOff() && r.runtime.model().screen==ScreenId::Home);
    r.run(r.hal.time+5*Second);    // 27.3s since the last read
    CHECK(hub.snapshot().count==1 && hub.snapshot().items[0].appId==LaunchTargetId::Timer);
    r.run(r.hal.time+4*Second);    // past 30s: read on a clock frame
    const auto& s=hub.snapshot();
    CHECK(s.count==2 && s.items[0].appId==LaunchTargetId::Timer && s.items[1].appId==LaunchTargetId::Pedometer);
    CHECK(std::string(s.items[1].label)=="10.0K");
}
void waitsAfterAFailedRead() {
    // A read that fails is not tried again on every frame of the clock: the
    // 30s run from the attempt, as they do from a read.
    PedometerHal hal;
    PedometerService service(hal); service.begin(0,0,0,false);
    CHECK(hal.reads==1 && service.lastReadAt()==0);
    hal.imu=false;
    CHECK(!service.refreshIfOlder(30*Second,30*Second) && hal.reads==2);
    for (TimeUs t=30*Second+16000;t<31*Second;t+=16000) service.refreshIfOlder(t,30*Second);
    CHECK(hal.reads==2);
    CHECK(!service.refreshIfOlder(60*Second-1,30*Second) && hal.reads==2);
    CHECK(!service.refreshIfOlder(60*Second,30*Second) && hal.reads==3);
    hal.imu=true;
    CHECK(service.refreshIfOlder(90*Second,30*Second) && hal.reads==4 && service.lastReadAt()==90*Second);
    // The screen's own second and a save still read when asked.
    hal.imu=false;
    CHECK(!service.refresh(90*Second+Second) && hal.reads==5);
}
void writesNothingIntoNothing() {
    char untouched[2]={'x','y'};
    formatSteps({true,12345},untouched,0); CHECK(untouched[0]=='x' && untouched[1]=='y');
    formatSteps({false,0},untouched,0); CHECK(untouched[0]=='x');
    formatSteps({true,12345},nullptr,0);
}
void savesOnTheNewDay() {
    // A save that lands just after 04:00, before any step ended the day (a
    // boot committed then): the day ends first, so the morning's steps are
    // kept under the new day's number, not yesterday's.
    Rig r(jstUs(2026,10,7,3,59,59));
    auto& p=r.pedometer;
    r.hal.steps=100; p.refresh(r.hal.time);
    CHECK(p.day()==dayOf(2026,10,6));
    r.hal.time+=2*Second; r.hal.steps=130;
    CHECK(r.app.pedometerRoutine().save(r.hal.time)==PrefResult::Ok);
    int32_t day=0; uint32_t steps=0;
    CHECK(p.day()==dayOf(2026,10,7));
    CHECK(r.records.saved(day,steps) && day==int32_t(dayOf(2026,10,7)) && steps==0);
    r.hal.steps=150;
    CHECK(r.app.pedometerRoutine().save(r.hal.time+Second)==PrefResult::Ok);
    CHECK(r.records.saved(day,steps) && day==int32_t(dayOf(2026,10,7)) && steps==20);
}
}
int main() {
    numbersDays(); plansAlignments(); keepsTheRecord(); counts(); carriesOverRestarts(); savesGoingDark();
    endsTheDayInTheDark(); retriesTheRead(); followsTheClock(); readsBesideTheClock(); savesForAnExternalBoot();
    formatsTheCount(); sitsAfterTheTimer(); showsTheCount(); opensWithoutAnImu(); labelsTheFace(); reachesTheFaceLast();
    waitsAfterAFailedRead(); writesNothingIntoNothing(); savesOnTheNewDay();
    std::cout << "PASS: day numbers, alignment plan, record, counting, restarts, save going dark, "
                 "day end in the dark, read retry, clock changes, reads beside the clock, external boot, "
                 "count format, list position, pedometer screen, screen without an IMU, face label, face order, "
                 "retry after a failed read, empty buffer, save on the new day\n";
}
