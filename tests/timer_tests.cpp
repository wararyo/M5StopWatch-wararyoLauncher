#include "host/HostApplication.h"
#include "TestScreens.h"
#include "host/LaunchRegistry.h"
#include "features/timer/TimerVibration.h"
#include "features/timer/TimerLayout.h"
#include "i18n/Strings.h"
#include "multifirm/FakeSlotService.h"
#include "services/TimeService.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
constexpr TimeUs Second=1000000,Minute=60*Second,Hour=60*Minute;
struct FakeHal : Hal, RenderPort, DisplayDataSource {
    TimeUs time=0,waited=0;
    InputSnapshot input{};
    UsbState usb{};
    int draws=0,sleeps=0,wakes=0;
    bool lightSleep=false,events=false;
    FrameModel rendered{};
    // Every level the runtime asked the motor for, and whether writes land.
    std::vector<int> levels;
    bool motorWrites=true;
    // The write reaches the motor, but the HAL reports a failure (written,
    // then not read back): the level the runtime must not assume.
    bool motorLandsButFails=false;
    int motor=0;
    TimeUs now() override { return time; }
    bool readRtc(CivilTime&) override { return false; }
    bool writeRtc(const CivilTime&) override { return false; }
    void setUtcClock(int64_t) override {}
    int64_t utcClockUs() override { return 0; }
    BatteryState sampleBattery() override { return {}; }
    void setBrightness(int) override {}
    InputSnapshot sampleInput() override { return input; }
    UsbState sampleUsb() override { return usb; }
    void setLightSleepAllowed(bool allowed) override { lightSleep=allowed; }
    void setScreenOff(bool off) override { off ? ++sleeps : ++wakes; }
    void invalidate() override {}
    void draw(const FrameModel& m,const WatchData&) override { ++draws; rendered=m; }
    TimeUs nextUpdate(TimeUs,const WatchData&) const override { return INT64_MAX; }
    void waitUs(TimeUs delay) override { waited=delay; time+=delay; }
    bool inputPending() override { return input.a || input.b || input.touching; }
    bool usbEvents() const override { return events; }
    bool setVibration(uint8_t level) override {
        levels.push_back(level);
        if (motorWrites || motorLandsButFails) motor=level;
        return motorWrites && !motorLandsButFails;
    }
};
// A monotonic clock that gains `ppm` while light sleep is allowed, as the
// device's does on its RC oscillator, and a data source that aligns it as
// TimeService does: the edge comes a poll after the alignment begins, and
// tells what was gained since the last one. The boot alignment measures
// nothing.
struct DriftingHal : FakeHal {
    TimeUs ppm=7500;            // 9s in 20 minutes, as seen on 2026-10-06.
    TimeUs gained=0,gainedAtEdge=-1;
    TimeUs edgeAt=INT64_MAX;
    int alignments=0;
    void waitUs(TimeUs delay) override {
        FakeHal::waitUs(delay);
        if (lightSleep) gained+=delay*ppm/(1000000+ppm);
    }
    // What a crystal would read, from the same origin.
    TimeUs real() const { return time-gained; }
    void alignClock(TimeUs now) override { ++alignments; edgeAt=now+TimeService::AlignPollUs; }
    TimeUs service(TimeUs now,ClockAlignment& a) override {
        a={};
        if (edgeAt==INT64_MAX) return INT64_MAX;
        if (now<edgeAt) return edgeAt;
        a.stepped=true;
        if (gainedAtEdge>=0) { a.measured=true; a.gainUs=gained-gainedAtEdge; }
        gainedAtEdge=gained; edgeAt=INT64_MAX;
        return INT64_MAX;
    }
};
// The timer's own record, kept in memory.
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
};

void normalizes() {
    CHECK(normalizeTimer(1,75,60)==2*3600+16*60);      // 02:16:00
    CHECK(normalizeTimer(0,0,99)==99);                 // 00:01:39
    CHECK(normalizeTimer(99,99,99)==TimerMaxSeconds);  // Capped at 99:59:59.
    CHECK(normalizeTimer(99,59,59)==TimerMaxSeconds);
    CHECK(normalizeTimer(0,0,0)==0);
    CHECK(normalizeTimer(-1,-5,3)==3);
}
void countsDown() {
    TimerService t;
    CHECK(t.state()==TimerState::Idle && t.deadline()==INT64_MAX);
    CHECK(!t.start(0,0) && !t.start(0,TimerMaxSeconds+1)); // Nothing to count.
    CHECK(t.state()==TimerState::Idle);
    CHECK(t.start(1000,10));
    CHECK(t.state()==TimerState::Running && t.duration()==10 && t.deadline()==1000+10*Second);
    CHECK(!t.start(1000,20));                         // One timer at a time.
    CHECK(t.remaining(1000+3*Second)==7*Second);
    // Paused, the remainder stays put however long it waits.
    CHECK(t.pause(1000+3*Second) && t.state()==TimerState::Paused && t.deadline()==INT64_MAX);
    CHECK(t.remaining(1000+90*Second)==7*Second);
    CHECK(!t.pause(1000+90*Second));
    CHECK(t.resume(100*Second) && t.deadline()==107*Second);
    CHECK(t.remaining(100*Second+1)==7*Second-1);
    // Not quite yet, then exactly at the end.
    CHECK(!t.expire(107*Second-1) && t.state()==TimerState::Running);
    CHECK(!t.pause(107*Second));                       // Run out: no pause.
    CHECK(t.expire(107*Second) && t.state()==TimerState::Ringing);
    CHECK(!t.expire(108*Second));                     // Once.
    CHECK(t.deadline()==INT64_MAX && t.remaining(108*Second)==0);
    CHECK(t.overrun(109*Second+500000)==2*Second+500000);
    CHECK(!t.reset() && t.state()==TimerState::Ringing);
    CHECK(t.dismiss() && t.state()==TimerState::Idle && !t.dismiss());
    // Noticed late, it still rings from its end.
    CHECK(t.start(0,5));
    CHECK(t.expire(9*Second) && t.expiredAt()==5*Second && t.overrun(9*Second)==4*Second);
    t.cancel();
    CHECK(t.state()==TimerState::Idle);
    // Reset from running or paused.
    CHECK(t.start(0,5) && t.reset() && t.state()==TimerState::Idle);
    CHECK(t.start(0,5) && t.pause(Second) && t.reset() && t.state()==TimerState::Idle);
}
// Setting the date moves the system clock, never the monotonic one the timer
// counts on.
void ignoresTheDate() {
    struct ClockHal : FakeHal {
        CivilTime rtc{2026,10,6,3,0,0};
        int64_t utc=0;
        bool readRtc(CivilTime& out) override { out=rtc; return true; }
        bool writeRtc(const CivilTime& in) override { rtc=in; return true; }
        void setUtcClock(int64_t seconds) override { utc=seconds*Second; }
        int64_t utcClockUs() override { return utc; }
    } hal;
    TimeService time; time.begin(hal);
    TimerService t;
    CHECK(t.start(0,600));
    CHECK(time.save({2027,1,1,12,0,0})==SaveResult::Saved);
    CHECK(t.remaining(60*Second)==540*Second && t.deadline()==600*Second);
}
// Where a running timer has the clock aligned (docs/task12/plan.md 2.1), as
// what is left then, each alignment begun when it is due, `late` after.
std::vector<TimeUs> alignmentsFor(int32_t seconds,TimeUs late=0) {
    TimerService t; t.start(0,seconds);
    std::vector<TimeUs> left;
    while (t.alignmentDue()!=INT64_MAX) {
        left.push_back(t.deadline()-t.alignmentDue());
        t.alignmentBegun(t.alignmentDue()+late);
    }
    return left;
}
void plansAlignments() {
    using V=std::vector<TimeUs>;
    CHECK(timerAlignmentLeft(2*Hour)==Hour && timerAlignmentLeft(2*Hour-1)==10*Minute);
    CHECK(timerAlignmentLeft(20*Minute)==10*Minute && timerAlignmentLeft(20*Minute-1)==Minute);
    CHECK(timerAlignmentLeft(2*Minute)==Minute && timerAlignmentLeft(2*Minute-1)==0);
    // From two hours on, an hour on; from 20 minutes, ten before; from two,
    // one before.
    CHECK(alignmentsFor(3*3600)==(V{2*Hour,Hour,10*Minute,Minute}));
    CHECK(alignmentsFor(2*3600+59*60)==(V{Hour+59*Minute,10*Minute,Minute}));
    CHECK(alignmentsFor(2*3600+5*60)==(V{Hour+5*Minute,10*Minute,Minute}));
    CHECK(alignmentsFor(3600+59*60)==(V{10*Minute,Minute}));
    CHECK(alignmentsFor(20*60)==(V{10*Minute,Minute}));
    CHECK(alignmentsFor(120)==(V{Minute}));
    CHECK(alignmentsFor(119).empty());
    // Begun late, an alignment is still the one planned.
    CHECK(alignmentsFor(3*3600,10000)==(V{2*Hour,Hour,10*Minute,Minute}));
    // The longest: every hour down to 1:59:59, then the last two.
    const auto longest=alignmentsFor(TimerMaxSeconds);
    CHECK(longest.size()==100 && longest.front()==TimeUs(TimerMaxSeconds)*Second-Hour);
    CHECK(longest[97]==Hour+59*Minute+59*Second && longest[98]==10*Minute);
    // Begun early (the panel came on), from what is left then. Paused, none;
    // resumed, planned again.
    TimerService t; t.start(0,3*3600);
    t.alignmentBegun(30*Minute);
    CHECK(t.alignmentDue()==30*Minute+Hour);
    CHECK(t.pause(40*Minute) && t.alignmentDue()==INT64_MAX);
    CHECK(t.resume(50*Minute) && t.alignmentDue()==50*Minute+Hour);
    // Run out, none.
    CHECK(t.expire(t.deadline()) && t.alignmentDue()==INT64_MAX);
}
// What the clock gained in the dark belongs to whatever was counting when the
// alignment began.
void takesCorrections() {
    TimerService t; t.start(0,1200);
    t.alignmentBegun(600*Second);
    t.clockCorrected(4500000);
    CHECK(t.deadline()==1204500000 && t.remaining(600*Second)==604500000);
    CHECK(t.alignmentDue()==t.deadline()-Minute);  // The plan moves with the end.
    t.clockCorrected(4500000);                       // Once per alignment.
    CHECK(t.deadline()==1204500000);
    // Paused since it began: what is left takes it.
    t.alignmentBegun(700*Second); CHECK(t.pause(700*Second+500000));
    const TimeUs left=t.remaining(0);
    t.clockCorrected(300000); CHECK(t.remaining(0)==left+300000);
    // Paused when it began: the dark it gained in was not counted.
    t.alignmentBegun(800*Second); t.clockCorrected(5*Second); CHECK(t.remaining(0)==left+300000);
    // Started, or reset, since it began: none of it is the new count's.
    TimerService u; u.alignmentBegun(0); CHECK(u.start(100000,60));
    u.clockCorrected(Second); CHECK(u.deadline()==100000+60*Second);
    u.alignmentBegun(Second); CHECK(u.reset() && u.start(2*Second,60));
    u.clockCorrected(Second); CHECK(u.deadline()==62*Second);
    // Ringing, the count up stays from the end it rang at.
    CHECK(u.expire(62*Second)); u.alignmentBegun(62*Second); u.clockCorrected(Second);
    CHECK(u.expiredAt()==62*Second && u.overrun(63*Second)==Second);
    // Behind, a paused remainder is never used up.
    TimerService v; CHECK(v.start(0,1)); v.alignmentBegun(0); CHECK(v.pause(500000));
    v.clockCorrected(-Second); CHECK(v.remaining(0)==1);
}
void vibrates() {
    // Weak taps, then longer ones, then medium pairs and long pulses, then
    // strong, for one minute. Times in milliseconds.
    auto at=[](TimeUs ms) { return timerVibration(ms*1000); };
    CHECK(at(0).level==VibrationWeak && at(0).until==60000);
    CHECK(at(60).level==0 && at(60).until==Second);
    CHECK(at(1000).level==VibrationWeak && at(1000).until==1060000);
    CHECK(at(7999).level==0 && at(7999).until==8*Second);
    CHECK(at(8000).level==VibrationWeak && at(8000).until==8100000);
    CHECK(at(8100).level==0 && at(8100).until==9*Second);
    CHECK(at(15999).level==0 && at(15999).until==16*Second);
    CHECK(at(16000).level==VibrationMedium && at(16000).until==16200000);
    CHECK(at(16300).level==0 && at(16300).until==16500000);
    CHECK(at(16500).level==VibrationMedium && at(16500).until==16700000);
    CHECK(at(16700).level==0 && at(16700).until==18*Second);
    CHECK(at(24000).level==VibrationMedium && at(24000).until==24500000);
    CHECK(at(24500).level==0 && at(24500).until==25*Second);
    CHECK(at(32000).level==VibrationStrong && at(32000).until==32400000);
    CHECK(at(32400).level==0 && at(32400).until==32600000);
    CHECK(at(59900).level==VibrationStrong && at(59900).until==60*Second); // Cut at the end.
    CHECK(at(60000).level==0 && at(60000).until==INT64_MAX);
    CHECK(at(-1000).level==VibrationWeak);
    // Followed change by change: always forward, every pulse under a second
    // (M5IOE1 must not sleep under one), and over at one minute.
    TimeUs t=0; int changes=0; TimeUs longest=0;
    while (true) {
        const auto step=timerVibration(t);
        if (step.until==INT64_MAX) break;
        CHECK(step.until>t);
        if (step.level) longest=std::max(longest,step.until-t);
        t=step.until; ++changes;
    }
    CHECK(t==TimerNoticeUs && longest<Second && changes>100);
}
void remembers() {
    MemoryRecords records;
    TimerPreferences p;
    // Unbound: the default, remembered in RAM only.
    CHECK(p.load()==PrefResult::Unavailable && p.value()==TimerPreferences::DefaultSeconds);
    CHECK(p.remember(30)==PrefResult::Unavailable && p.value()==30);
    p.bind(&records);
    CHECK(p.load()==PrefResult::Missing && p.value()==TimerPreferences::DefaultSeconds && records.writes==0);
    // The normalized length: 01:75:60 is stored as 02:16:00.
    CHECK(p.remember(normalizeTimer(1,75,60))==PrefResult::Ok && records.writes==1);
    CHECK(p.remember(8160)==PrefResult::Ok && records.writes==1);   // Unchanged: no write.
    TimerPreferences reopened; reopened.bind(&records);
    CHECK(reopened.load()==PrefResult::Ok && reopened.value()==8160);
    // A failed write keeps the value for this run and compares with what landed.
    records.writable=false;
    CHECK(p.remember(100)==PrefResult::WriteFailed && p.value()==100 && records.writes==2);
    CHECK(p.remember(8160)==PrefResult::Ok && records.writes==2);   // Storage still holds it.
    CHECK(p.remember(100)==PrefResult::WriteFailed && records.writes==3);
    records.writable=true;
    CHECK(p.remember(100)==PrefResult::Ok && records.writes==4);
    CHECK(p.remember(0)==PrefResult::TooLarge && p.value()==100);
    // Unknown or broken records: the default, and left alone.
    for (const auto& bad:std::vector<std::vector<uint8_t>>{
             {2,10,0,0,0},{1,0,0,0,0},{1,0xff,0xff,0xff,0x7f},{1,10,0,0},{1,10,0,0,0,0}}) {
        MemoryRecords broken; broken.records["timer"]=bad;
        TimerPreferences q; q.bind(&broken);
        CHECK(q.load()==PrefResult::Invalid && q.value()==TimerPreferences::DefaultSeconds && broken.writes==0);
    }
    // The key is the host's: no face can take it.
    CHECK(reservedRecordKey("timer"));
    WatchPreferences faces; faces.bind(&records);
    const uint8_t payload[1]{1};
    CHECK(faces.saveFace("timer",payload,1)==PrefResult::Invalid);
}

// The runtime around a timer: built like the device's.
template<class H> struct RigOf {
    H hal;
    HostApplication app{hal,hal,hal,468,468};
    HostRuntime& runtime=app.runtime();
    TimerService& timer=app.timer();
    RigOf() { runtime.begin(); runtime.step(); }
    void at(TimeUs t) { hal.time=t; runtime.step(); }
    // Steps at the input period over [from, to], as a held button is followed.
    void follow(TimeUs from,TimeUs to) { for (TimeUs t=from;t<=to;t+=10000) at(t); }
    // A button pressed for `held`, released, and the loop left to settle.
    void press(bool a,TimeUs held=20000) {
        (a ? hal.input.a : hal.input.b)=true; follow(hal.time+10000,hal.time+held);
        (a ? hal.input.a : hal.input.b)=false; at(hal.time+10000);
        at(hal.time+200000);
    }
    ScreenId screen() { return runtime.model().screen; }
    TimerModel shown() { return runtime.model().timer; }
};
using Rig=RigOf<FakeHal>;
void alertsWhileDark() {
    Rig r; r.hal.events=true; r.hal.usb={true,0,false};   // On battery.
    CHECK(r.timer.start(r.hal.time,60));
    const TimeUs end=r.timer.deadline();
    r.at(31*Second); r.at(32*Second);
    CHECK(r.runtime.power().screenOff() && r.hal.lightSleep);
    // Asleep, the wait still ends at the expiry.
    r.runtime.wait();
    CHECK(r.hal.time==end);
    r.runtime.step();
    // The timer screen comes forward, ringing, and is the alert.
    CHECK(r.timer.state()==TimerState::Ringing && r.screen()==ScreenId::Timer);
    CHECK(r.shown().view==TimerView::Ringing && r.shown().seconds==0);
    CHECK(!r.runtime.power().screenOff() && r.hal.wakes==1 && !r.hal.lightSleep);
    CHECK(r.hal.motor==VibrationWeak);
    // The motor follows the pattern on the screen's deadlines. The panel's
    // fade wakes the loop as well, so it is followed by step.
    r.at(end+60000-1); CHECK(r.hal.motor==VibrationWeak);
    r.at(end+60000); CHECK(r.hal.motor==0);
    r.at(end+Second-1); CHECK(r.hal.motor==0 && r.shown().seconds==0);
    r.at(end+Second); CHECK(r.hal.motor==VibrationWeak && r.shown().seconds==1);
    // Lit for the whole minute, rests included, though nothing is pressed.
    for (TimeUs t=end+3*Second;t<end+60*Second;t+=3*Second) { r.at(t); CHECK(!r.runtime.power().screenOff()); }
    r.at(end+60*Second);
    CHECK(r.hal.motor==0 && r.timer.state()==TimerState::Ringing && r.shown().seconds==60);
    // Then the usual timeout, counted from the end of the alert.
    r.at(end+90*Second-1); CHECK(!r.runtime.power().screenOff());
    r.at(end+90*Second); CHECK(r.runtime.power().screenOff() && r.hal.sleeps==2);
    // Still ringing: nothing restarts the alert, and the dark screen asks for
    // no frames.
    const auto writes=r.hal.levels.size();
    r.at(end+200*Second); CHECK(r.hal.levels.size()==writes && r.runtime.power().screenOff());
    r.hal.time=end+300*Second; r.runtime.wait(); CHECK(r.hal.waited>Second);
    // Woken by a touch (spent on the wake), it still rings, quietly.
    r.hal.input={false,false,true,234,234}; r.at(end+400*Second);
    r.hal.input={}; r.at(end+400*Second+10000);
    CHECK(r.screen()==ScreenId::Timer && r.shown().view==TimerView::Ringing && r.hal.motor==0);
}
// On battery and dark, a 20-minute timer wakes ten minutes and one minute
// before its end to align the clock, without lighting the panel, and rings
// early only by what the last minute gained (docs/task12/plan.md 2.1).
void alignsWhileDark() {
    RigOf<DriftingHal> r; r.hal.events=true; r.hal.usb={true,0,false};
    const TimeUs start=r.hal.time;
    CHECK(r.timer.start(start,1200));
    r.at(31*Second); r.at(32*Second);
    CHECK(r.runtime.power().screenOff() && r.hal.lightSleep);
    const int alignments=r.hal.alignments;
    r.runtime.wait();
    CHECK(r.hal.time==start+600*Second);
    r.runtime.step();
    CHECK(r.hal.alignments==alignments+1 && r.hal.wakes==0 && r.hal.lightSleep);
    // A poll later the edge tells what the dark added, and the end moves by it.
    const TimeUs end=r.timer.deadline();
    r.runtime.wait(); CHECK(r.hal.time==start+600*Second+TimeService::AlignPollUs);
    r.runtime.step();
    CHECK(r.hal.gained>4*Second && r.timer.deadline()==end+r.hal.gained);
    CHECK(r.timer.alignmentDue()==r.timer.deadline()-Minute);
    r.runtime.wait(); CHECK(r.hal.time==r.timer.deadline()-Minute);
    r.runtime.step(); r.runtime.wait(); r.runtime.step();
    CHECK(r.hal.alignments==alignments+2 && r.hal.wakes==0 && r.hal.lightSleep);
    // Uncorrected it would ring 9s early; now by about half a second.
    r.runtime.wait(); CHECK(r.hal.time==r.timer.deadline());
    r.runtime.step();
    CHECK(r.timer.state()==TimerState::Ringing && r.hal.wakes==1);
    const TimeUs early=start+1200*Second-r.hal.real();
    CHECK(early>=0 && early<500000);
}
// The panel coming on aligns the clock too, and the next alignment is planned
// from there.
void replansOnWake() {
    Rig r; r.hal.events=true; r.hal.usb={true,0,false};
    const TimeUs start=r.hal.time;
    CHECK(r.timer.start(start,3*3600) && r.timer.alignmentDue()==start+Hour);
    r.at(31*Second); r.at(32*Second);
    CHECK(r.runtime.power().screenOff());
    r.hal.input={false,false,true,234,234}; r.at(start+30*Minute);
    r.hal.input={}; r.at(start+30*Minute+10000);
    CHECK(!r.runtime.power().screenOff() && r.timer.alignmentDue()==start+90*Minute);
    r.at(start+31*Minute);
    CHECK(r.runtime.power().screenOff());
    r.runtime.wait(); CHECK(r.hal.time==start+90*Minute);
    r.runtime.step();
    CHECK(r.timer.alignmentDue()==r.timer.deadline()-10*Minute && r.hal.wakes==1);
}
// A countdown on screen shows the corrected end as soon as it is known, and
// times its next second from it.
void showsTheCorrection() {
    RigOf<DriftingHal> r; r.hal.events=true; r.hal.usb={true,0,false};
    const TimeUs start=r.hal.time;
    CHECK(r.timer.start(start,3600) && r.app.screens().present(ScreenId::Timer,start));
    r.at(31*Second);
    CHECK(r.runtime.power().screenOff() && r.hal.lightSleep);
    r.runtime.wait(start+6*Minute); CHECK(r.hal.time==start+6*Minute);
    r.hal.input={false,false,true,234,234}; r.at(start+6*Minute);
    const int before=r.shown().seconds;
    CHECK(r.shown().view==TimerView::Countdown && before==3600-6*60);
    r.hal.input={}; r.at(start+6*Minute+TimeService::AlignPollUs);
    CHECK(r.timer.deadline()==start+3600*Second+r.hal.gained);
    CHECK(r.shown().seconds==timerShownRemaining(r.timer.remaining(r.hal.time)) && r.shown().seconds>before+1);
    CHECK(r.app.screens().nextUpdate()==r.timer.deadline()-TimeUs(r.shown().seconds-1)*Second);
}
// Dismissed early, the motor stops and the timeout runs from the input that
// dismissed it, not from the alert's end. The setup comes back with the length
// last started.
void dismissEndsTheAlert() {
    Rig r;
    CHECK(r.timer.start(r.hal.time,5));
    r.at(5*Second);
    CHECK(r.hal.motor==VibrationWeak);
    r.hal.input.a=true; r.at(10*Second);
    r.hal.input.a=false; r.at(10*Second+10000);
    CHECK(r.timer.state()==TimerState::Idle && r.hal.motor==0);
    CHECK(r.screen()==ScreenId::Timer && r.shown().view==TimerView::Setup);
    r.at(40*Second); CHECK(!r.runtime.power().screenOff());
    r.at(40*Second+10000); CHECK(r.runtime.power().screenOff());
    // Home dismisses it too, and leaves for the clock.
    Rig h;
    CHECK(h.timer.start(h.hal.time,5));
    h.at(5*Second);
    h.hal.input.a=h.hal.input.b=true; h.follow(6*Second,7*Second);
    h.hal.input.a=h.hal.input.b=false; h.at(7*Second+10000);
    CHECK(h.screen()==ScreenId::Home && h.timer.state()==TimerState::Idle && h.hal.motor==0);
}
// A write that does not land is sent again, during the alert and after it.
void retriesTheMotor() {
    Rig r;
    CHECK(r.timer.start(r.hal.time,5));
    r.at(5*Second); r.at(5*Second+60000);
    CHECK(r.hal.motor==0);
    // Followed in a strong pulse, which outlasts a retry.
    const TimeUs pulse=5*Second+32*Second;
    const auto written=r.hal.levels.size();
    r.hal.motorWrites=false;
    r.at(pulse);
    CHECK(r.hal.motor==0 && r.hal.levels.size()==written+1);
    r.at(pulse+49000); CHECK(r.hal.levels.size()==written+1);
    r.at(pulse+50000); CHECK(r.hal.levels.size()==written+2);
    r.hal.motorWrites=true; r.at(pulse+100000);
    CHECK(r.hal.motor==VibrationStrong);
    // Stopping fails at first too: it is retried after the alert is gone.
    r.hal.motorWrites=false;
    r.hal.input.b=true; r.at(pulse+110000);
    r.hal.input.b=false; r.at(pulse+120000);
    CHECK(r.timer.state()==TimerState::Idle && r.hal.levels.back()==0 && r.hal.motor==VibrationStrong);
    r.hal.motorWrites=true; r.at(pulse+170000);
    CHECK(r.hal.motor==0);
    const auto writes=r.hal.levels.size();           // Nothing left to retry.
    r.at(r.hal.time+Second); r.at(r.hal.time+Second);
    CHECK(r.hal.levels.size()==writes);
    // A write that reached the motor but reported a failure leaves the level
    // unknown: dismissed before any retry, the stop is still written, and
    // written again until a write is confirmed (review 2026-10-07, P1).
    Rig u;
    CHECK(u.timer.start(u.hal.time,5));
    u.hal.motorLandsButFails=true;
    u.at(5*Second);
    CHECK(u.hal.motor==VibrationWeak && u.hal.levels.size()==1);
    u.hal.input.b=true; u.at(5*Second+10000);
    u.hal.input.b=false; u.at(5*Second+20000);
    // The stop waits only for the retry after the failed write.
    CHECK(u.timer.state()==TimerState::Idle && u.hal.levels.size()==1 && u.hal.motor==VibrationWeak);
    u.at(5*Second+50000); CHECK(u.hal.levels.size()==2 && u.hal.levels.back()==0 && u.hal.motor==0);
    u.at(5*Second+100000); CHECK(u.hal.levels.size()==3 && u.hal.levels.back()==0);
    u.hal.motorLandsButFails=false; u.at(5*Second+150000);
    CHECK(u.hal.motor==0 && u.hal.levels.size()==4);
    const auto settled=u.hal.levels.size();
    u.at(u.hal.time+Second); u.at(u.hal.time+Second);
    CHECK(u.hal.levels.size()==settled);
}
// The expiry is settled before the input of the same step: a release that
// arrives with it reaches nothing (docs/task12/plan.md 2.4).
void expiryBeforeInput() {
    // B released in the step the timer runs out, on the list: no launch, and
    // the alert it brings forward is not dismissed by it.
    {
        Rig r;
        r.press(true);
        CHECK(r.screen()==ScreenId::AppList);
        CHECK(r.timer.start(r.hal.time,2));
        const TimeUs end=r.timer.deadline();
        r.hal.input.b=true; r.follow(end-300000,end-10000);
        r.hal.input.b=false; r.at(end);
        CHECK(r.timer.state()==TimerState::Ringing && r.screen()==ScreenId::Timer);
        r.at(end+10000);
        CHECK(r.timer.state()==TimerState::Ringing);
        // The next press is new and dismisses.
        r.press(false);
        CHECK(r.timer.state()==TimerState::Idle && r.shown().view==TimerView::Setup);
    }
    // A+B held across the expiry: no home from that hold.
    {
        Rig r;
        r.press(true);
        CHECK(r.timer.start(r.hal.time,1));
        const TimeUs end=r.timer.deadline();
        r.hal.input.a=r.hal.input.b=true; r.follow(end-200000,end+1000000);
        CHECK(r.screen()==ScreenId::Timer && r.runtime.model().homeCount==0);
        CHECK(r.timer.state()==TimerState::Ringing);
        r.hal.input.a=r.hal.input.b=false; r.at(end+1010000);
        // Pressed again, it is home.
        r.hal.input.a=r.hal.input.b=true; r.follow(end+1100000,end+1800000);
        CHECK(r.screen()==ScreenId::Home && r.runtime.model().homeCount==1);
    }
    // A pause that arrives late does not land on a timer that ran out: the
    // service refuses, whatever the caller.
    {
        TimerService t; t.start(0,1);
        CHECK(!t.pause(Second) && t.expire(Second));
    }
}
// An external boot ends the timer and the motor; a boot that fails leaves the
// timer alone (docs/task12/plan.md 1, 2.4).
void externalBoot() {
    for (bool succeeds:{true,false}) {
        Rig r;
        FakeSlotService slots; slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
        slots.bootSucceeds=succeeds;
        r.app.bindSlots(slots);
        CHECK(r.timer.start(r.hal.time,600));
        r.press(true);
        while (LaunchRegistry[r.runtime.model().launcher.list.selection].id!=LaunchTargetId::External1) r.press(true);
        r.press(false);
        CHECK(slots.bootRequests==1);
        if (succeeds) CHECK(r.timer.state()==TimerState::Idle && r.hal.levels.back()==0);
        else CHECK(r.timer.state()==TimerState::Running);
    }
    // Ringing as well, with the motor running.
    TimerService timer; StopwatchService stopwatch; FakeHal hal;
    HostShutdown shutdown(stopwatch,timer,hal);
    timer.start(0,1); timer.expire(Second);
    shutdown.onBootCommitted();
    CHECK(timer.state()==TimerState::Idle && hal.levels.size()==1 && hal.levels[0]==0);
}
// The host side alone, with a source of the test's own: what any feature that
// asks for attention gets (host/AttentionSource.h).
struct FakeSource : AttentionSource {
    AttentionRequest request{};
    TimeUs next=INT64_MAX;
    int asked=0;
    AttentionRequest attention(TimeUs) override { ++asked; return request; }
    TimeUs nextAttention() const override { return next; }
};
void attentionContract() {
    Rig r;
    FakeSource source;
    CHECK(r.runtime.bindAttention(source));
    // Waited for with the panel dark, like a timer's end.
    r.at(31*Second);
    CHECK(r.runtime.power().screenOff());
    source.next=100*Second;
    r.runtime.wait(); CHECK(r.hal.time<=100*Second);
    // Started before the input: B held from before is spent, the panel
    // lights and the screen is presented.
    r.hal.input.b=true; r.at(99*Second);
    source.request={true,true,ScreenId::Stopwatch};
    source.next=INT64_MAX;
    r.hal.input.b=false; r.at(100*Second);
    CHECK(!r.runtime.power().screenOff() && r.screen()==ScreenId::Stopwatch);
    CHECK(r.app.stopwatch().state()==StopwatchState::Reset); // The release started nothing.
    // Started once: a request that goes on is not presented again.
    r.press(true);                                        // LAP: nothing in Reset, but handled.
    r.hal.input.a=r.hal.input.b=true; r.follow(r.hal.time+10000,r.hal.time+700000);
    r.hal.input.a=r.hal.input.b=false; r.at(r.hal.time+10000);
    CHECK(r.screen()==ScreenId::Home);
    r.at(r.hal.time+Second);
    CHECK(r.screen()==ScreenId::Home);
    // Ended and asked again: it starts again.
    source.request.active=false; r.at(r.hal.time+Second);
    source.request.active=true; r.at(r.hal.time+Second);
    CHECK(r.screen()==ScreenId::Stopwatch);
    // A request that does not present only lights the panel.
    source.request={}; r.at(r.hal.time+Second);
    r.at(r.hal.time+40*Second); CHECK(r.runtime.power().screenOff());
    source.request={true,false,ScreenId::Home}; r.at(r.hal.time+Second);
    CHECK(!r.runtime.power().screenOff() && r.screen()==ScreenId::Stopwatch);
    // Capacity: four sources in all, the timer's among them.
    FakeSource other,third,fourth;
    CHECK(r.runtime.bindAttention(other) && r.runtime.bindAttention(third) && !r.runtime.bindAttention(fourth));
}
// Presenting leaves what was shown as home does.
void presenting() {
    TestScreens screens;
    TimeUs now=0;
    Events e{}; e.next=true;
    screens.handle(e,now); now+=200000; screens.update(now);
    CHECK(screens.model().screen==ScreenId::AppList);
    CHECK(screens.present(ScreenId::Stopwatch,now) && screens.model().screen==ScreenId::Stopwatch);
    // Back to the list from there is a fresh visit, as after home.
    CHECK(screens.model().launcher.transition==0);
    // What cannot be presented changes nothing.
    CHECK(!screens.present(ScreenId::AppList,now) && !screens.present(ScreenId::External,now));
    CHECK(screens.model().screen==ScreenId::Stopwatch);
    // Settings is unavailable without its store: refused, nothing left.
    CHECK(!screens.present(ScreenId::Settings,now) && screens.model().screen==ScreenId::Stopwatch);
    CHECK(screens.present(ScreenId::Home,now) && screens.model().screen==ScreenId::Home);
    CHECK(screens.model().homeCount==0); // Not a home press.
    // The countdown shown when the timer rings: entered again, not left, so
    // the alert is not dismissed on its way in.
    screens.timer.start(now,5);
    CHECK(screens.present(ScreenId::Timer,now) && screens.model().timer.view==TimerView::Countdown);
    screens.timer.expire(now+5*Second);
    CHECK(screens.present(ScreenId::Timer,now+5*Second));
    CHECK(screens.model().timer.view==TimerView::Ringing && screens.timer.state()==TimerState::Ringing);
    CHECK(screens.vibration()==VibrationWeak && screens.holdPanelUntil()==now+5*Second+TimerNoticeUs);
}

// The screen on its own (docs/task12/plan.md 1.1-1.3), driven like the list
// drives it.
struct TimerRig {
    MemoryRecords records;
    TestScreens screens;
    TimeUs now=0;
    TimerRig(int32_t last=TimerPreferences::DefaultSeconds) {
        if (last!=TimerPreferences::DefaultSeconds) {
            uint8_t record[TimerPreferences::RecordBytes];
            TimerPreferences::encode(last,record);
            records.records["timer"].assign(record,record+sizeof(record));
        }
        screens.timerPreferences.bind(&records);
        screens.timerPreferences.load();
        open();
    }
    // From home, through the list, as the wearer opens it.
    void open() {
        Events home{}; home.home=true; screens.handle(home,now); now+=1000;
        Events next{}; next.next=true;
        screens.handle(next,now); now+=200000; screens.update(now);
        while (LaunchRegistry[screens.model().launcher.list.selection].id!=LaunchTargetId::Timer) {
            screens.handle(next,now); now+=200000; screens.update(now);
        }
        Events decide{}; decide.decide=true;
        screens.handle(decide,now); now+=1000;
        CHECK(screens.model().screen==ScreenId::Timer);
    }
    const TimerModel& t() { model=screens.model().timer; return model; }
    TimerModel model{};
    void a(TimeUs held=100000) { Events e{}; e.next=true; e.pressUs=held; screens.handle(e,now); }
    void b() { Events e{}; e.decide=true; e.pressUs=100000; screens.handle(e,now); }
    void tap(const Rect& box,TimeUs held=100000) {
        Events e{}; e.gesture=Gesture::Tap; e.x=box.x+box.w/2; e.y=box.y+box.h/2; e.touchUs=held;
        screens.handle(e,now);
    }
    void touch(const Rect& box) {
        Events e{}; e.gesture=Gesture::TouchStart; e.x=box.x+box.w/2; e.y=box.y+box.h/2;
        screens.handle(e,now);
    }
    void hold(Hold h) { Events e{}; e.holdChanged=true; e.hold=h; e.holdSince=now; screens.handle(e,now); }
    void advance(TimeUs to) { now=to; if (now>=screens.nextUpdate()) screens.update(now); }
    Viewport m() { return screens.viewport(); }
};
void setsUp() {
    TimerRig r;
    // Opened idle: the setup, from the default length, the minutes focused.
    CHECK(r.t().view==TimerView::Setup && r.t().fields[0]==0 && r.t().fields[1]==3 && r.t().fields[2]==0);
    CHECK(r.t().focus==1);
    // A moves the focus through the fields to SET and round.
    for (int i=1;i<=4;++i) { r.a(); CHECK(r.t().focus==(1+i)%TimerFocusCount); }
    // B steps a field: minutes and seconds 59 back to 0, hours 99 back to 0.
    const Viewport m=r.m();
    r.tap(timerFieldBox(m,0)); r.b(); CHECK(r.t().fields[0]==1);
    r.a(); for (int i=0;i<56;++i) r.b();
    CHECK(r.t().fields[1]==59);
    r.b(); CHECK(r.t().fields[1]==0);
    // Keys type into the focused field from the right.
    r.tap(timerKeyBox(m,7)); r.tap(timerKeyBox(m,5));
    CHECK(r.t().fields[1]==75);
    r.b(); CHECK(r.t().fields[1]==76);               // Over 59 it counts on to 99.
    r.tap(timerKeyBox(m,9)); r.tap(timerKeyBox(m,9)); r.b();
    CHECK(r.t().fields[1]==0);
    // The tall 0 takes touches from as far again to its right as it is wide,
    // and no further; what it shows stays the same.
    const Rect zero=timerKeyBox(m,0),zeroHit=timerKeyHitBox(m,0);
    CHECK(zeroHit.x==zero.x && zeroHit.y==zero.y && zeroHit.h==zero.h && zeroHit.w>=2*zero.w-1 && zeroHit.w<=2*zero.w+1);
    CHECK(zeroHit.x+zeroHit.w<=m.width);
    CHECK(hitTimer(m,TimerView::Setup,zero.x+zero.w+zero.w/2,zero.y+zero.h/2).kind==TimerHit::Key);
    CHECK(hitTimer(m,TimerView::Setup,zero.x+zero.w+zero.w/2,zero.y+zero.h/2).index==0);
    CHECK(hitTimer(m,TimerView::Setup,zeroHit.x+zeroHit.w,zero.y+zero.h/2).kind==TimerHit::None);
    for (int d=1;d<10;++d) CHECK(timerKeyHitBox(m,d)==timerKeyBox(m,d));
    // So does SET, below it.
    const Rect set=timerSetBox(m),setHit=timerSetHitBox(m);
    CHECK(setHit.x==set.x && setHit.y==set.y && setHit.w==set.w && setHit.h>=2*set.h-1 && setHit.h<=2*set.h+1);
    CHECK(hitTimer(m,TimerView::Setup,set.x+set.w/2,set.y+set.h+set.h/2).kind==TimerHit::Set);
    CHECK(hitTimer(m,TimerView::Setup,set.x+set.w/2,setHit.y+setHit.h).kind==TimerHit::None);
    // A tap focuses a field; with SET focused the keys do nothing.
    r.tap(timerFieldBox(m,2)); CHECK(r.t().focus==2);
    r.tap(timerKeyBox(m,0)); r.tap(timerKeyBox(m,6)); r.tap(timerKeyBox(m,0));
    CHECK(r.t().fields[2]==60);
    r.a(); CHECK(r.t().focus==TimerFocusSet);
    r.tap(timerKeyBox(m,4)); CHECK(r.t().fields[0]==1 && r.t().fields[1]==0 && r.t().fields[2]==60);
    // B on SET starts, carried: 01:00:60 is 01:01:00, shown as such, and kept.
    r.b();
    CHECK(r.screens.timer.state()==TimerState::Running && r.screens.timer.duration()==3660);
    CHECK(r.t().view==TimerView::Countdown && r.t().seconds==3660);
    CHECK(r.records.writes==1 && r.screens.timerPreferences.value()==3660);
    CHECK(r.screens.model().toast==nullptr);
    // Leaving keeps it running; coming back shows the countdown.
    r.open();
    CHECK(r.screens.timer.state()==TimerState::Running && r.t().view==TimerView::Countdown);
}
void setupEdges() {
    // B let go in the same sample as a tap: the hold ends there, and nothing
    // keeps counting with nothing held (review 2026-10-07, P2).
    {
        TimerRig r;
        r.hold(Hold::B);
        CHECK(r.t().fields[1]==4);
        const Rect minutes=timerFieldBox(r.m(),1);
        Events both{}; both.gesture=Gesture::Tap; both.x=minutes.x+minutes.w/2; both.y=minutes.y+minutes.h/2;
        both.touchUs=100000; both.decide=true; both.holdChanged=true; both.pressUs=100000;
        r.screens.handle(both,r.now);
        CHECK(r.t().fields[1]==4 && r.screens.nextUpdate()==INT64_MAX);
        r.advance(r.now+2*Second);
        CHECK(r.t().fields[1]==4);
        // A chord in the same sample as a tap ends it as well.
        r.hold(Hold::B);
        Events chord{}; chord.gesture=Gesture::Tap; chord.x=both.x; chord.y=both.y; chord.holdChanged=true;
        r.screens.handle(chord,r.now);
        CHECK(r.t().fields[1]==5 && r.screens.nextUpdate()==INT64_MAX);
    }
    // 00:00:00 starts nothing, from the button or by B.
    {
        TimerRig r;
        for (int i=0;i<3;++i) { r.tap(timerFieldBox(r.m(),i)); r.tap(timerKeyBox(r.m(),0)); r.tap(timerKeyBox(r.m(),0)); }
        r.tap(timerSetBox(r.m()));
        CHECK(r.screens.timer.state()==TimerState::Idle && r.t().view==TimerView::Setup);
        r.a(); CHECK(r.t().focus==TimerFocusSet);
        r.b();
        CHECK(r.screens.timer.state()==TimerState::Idle && r.records.writes==0);
    }
    // 99:99:99 is held at the longest; SET by touch starts whatever the focus.
    {
        TimerRig r;
        for (int i=0;i<3;++i) { r.tap(timerFieldBox(r.m(),i)); r.tap(timerKeyBox(r.m(),9)); r.tap(timerKeyBox(r.m(),9)); }
        r.tap(timerFieldBox(r.m(),0));
        r.tap(timerSetBox(r.m()));
        CHECK(r.screens.timer.duration()==TimerMaxSeconds && r.t().seconds==TimerMaxSeconds);
    }
    // The last length is the next starting point, across a reboot.
    {
        TimerRig r(95);
        CHECK(r.t().fields[0]==0 && r.t().fields[1]==1 && r.t().fields[2]==35);
    }
    // A failed write still starts, says so, and keeps the length for now.
    {
        TimerRig r;
        r.records.writable=false;
        r.tap(timerSetBox(r.m()));
        CHECK(r.screens.timer.state()==TimerState::Running && r.screens.model().toast!=nullptr);
        CHECK(std::string(r.screens.model().toast)==text::SaveFailed);
        CHECK(r.screens.timerPreferences.value()==180);
    }
    // B on a field steps as it goes down, then repeats while held; the
    // release adds nothing.
    {
        TimerRig r;
        r.hold(Hold::B);
        const TimeUs since=r.now;
        CHECK(r.t().fields[1]==4 && r.screens.nextUpdate()==since+HoldRepeat::DelayUs);
        r.advance(since+HoldRepeat::DelayUs); r.advance(since+HoldRepeat::DelayUs+HoldRepeat::PeriodUs);
        CHECK(r.t().fields[1]==6);
        Events release{}; release.decide=true; release.holdChanged=true; release.pressUs=700000;
        r.screens.handle(release,r.now);
        CHECK(r.t().fields[1]==6 && r.screens.nextUpdate()==INT64_MAX);
        // On SET nothing happens as B goes down; the release starts.
        r.a(); r.a(); CHECK(r.t().focus==TimerFocusSet);
        r.hold(Hold::B); CHECK(r.screens.timer.state()==TimerState::Idle);
        r.screens.handle(release,r.now);
        CHECK(r.screens.timer.state()==TimerState::Running && r.screens.timer.duration()==360);
    }
}
void countsDownOnScreen() {
    TimerRig r;
    r.tap(timerSetBox(r.m()));                       // 00:03:00
    const TimeUs start=r.now,end=r.screens.timer.deadline();
    CHECK(r.t().seconds==180);
    // Rounded up: the length shows until a whole second is gone, and the
    // frame comes exactly when the shown second changes.
    CHECK(r.screens.nextUpdate()==start+Second);
    r.advance(start+Second-1); CHECK(r.t().seconds==180);
    r.advance(start+Second); CHECK(r.t().seconds==179 && r.screens.nextUpdate()==start+2*Second);
    r.advance(end-400000); CHECK(r.t().seconds==1 && r.screens.nextUpdate()==end);
    // B pauses and resumes; paused, no frames.
    r.now=start+10*Second;
    r.b(); CHECK(r.t().paused && r.screens.timer.state()==TimerState::Paused && r.screens.nextUpdate()==INT64_MAX);
    r.now=start+100*Second;
    r.tap(timerCountdownButtonBox(r.m(),1)); CHECK(!r.t().paused && r.screens.timer.remaining(r.now)==170*Second);
    // A short does nothing; A held fills RESET.
    r.a(599999); CHECK(r.screens.timer.state()==TimerState::Running);
    r.hold(Hold::A);
    TimeUs held=r.now;
    CHECK(r.t().resetFill==0 && r.screens.nextUpdate()==held+16000);
    r.advance(held+300000); CHECK(r.t().resetFill==500);
    // B joining makes a chord: the fill empties and nothing resets.
    r.hold(Hold::None); CHECK(r.t().resetFill==0 && r.screens.nextUpdate()!=r.now+16000);
    r.advance(held+700000); CHECK(r.screens.timer.state()==TimerState::Running);
    // Held to 600ms it resets there and then, still held: the frame comes
    // exactly at 600ms, and the setup shows the length last started.
    r.hold(Hold::A); held=r.now;
    r.advance(held+590000); CHECK(r.screens.timer.state()==TimerState::Running);
    CHECK(r.screens.nextUpdate()==held+600000);
    r.advance(held+600000);
    CHECK(r.screens.timer.state()==TimerState::Idle && r.t().view==TimerView::Setup);
    CHECK(r.t().fields[1]==3 && r.t().focus==1);
    // Its release, later, does nothing to the setup (it would move the focus).
    r.now+=400000;
    Events release{}; release.next=true; release.holdChanged=true; release.pressUs=1000000;
    r.screens.handle(release,r.now);
    CHECK(r.t().focus==1);
    // The next A is a press of its own.
    r.a(); CHECK(r.t().focus==2);
}
void resetsByTouch() {
    TimerRig r;
    r.tap(timerSetBox(r.m()));
    const Rect reset=timerCountdownButtonBox(r.m(),0);
    // Lifted too soon: nothing.
    r.touch(reset); r.now+=300000; r.tap(reset,300000);
    CHECK(r.screens.timer.state()==TimerState::Running && r.t().resetFill==0);
    // Slid off: nothing either.
    r.touch(reset);
    Events drag{}; drag.gesture=Gesture::DragStart; r.screens.handle(drag,r.now);
    CHECK(r.t().resetFill==0);
    Events end{}; end.gesture=Gesture::DragEnd; r.now+=700000; r.screens.handle(end,r.now);
    CHECK(r.screens.timer.state()==TimerState::Running);
    // Held on RESET to 600ms: reset there and then, still touching.
    r.touch(reset);
    const TimeUs held=r.now;
    r.advance(held+300000); CHECK(r.t().resetFill==500);
    r.advance(held+600000);
    CHECK(r.screens.timer.state()==TimerState::Idle && r.t().view==TimerView::Setup);
    // Lifting the finger, over what is now the hours field, does nothing.
    r.now+=500000; r.tap(reset,1100000);
    CHECK(r.t().focus==1);
    // The next touch is a touch of its own.
    r.tap(timerFieldBox(r.m(),0)); CHECK(r.t().focus==0);
}
void ringsOnScreen() {
    TimerRig r;
    r.tap(timerSetBox(r.m()));
    const TimeUs end=r.screens.timer.deadline();
    r.screens.timer.expire(end);
    r.screens.present(ScreenId::Timer,end);
    r.now=end;
    CHECK(r.t().view==TimerView::Ringing && r.t().seconds==0);
    CHECK(r.screens.holdPanelUntil()==end+TimerNoticeUs && r.screens.vibration()==VibrationWeak);
    // Counted up, rounded down; the frame comes when the count changes or
    // the motor does.
    CHECK(r.screens.nextUpdate()==end+60000);
    r.advance(end+60000); CHECK(r.screens.vibration()==0 && r.screens.nextUpdate()==end+Second);
    r.advance(end+Second); CHECK(r.t().seconds==1);
    r.advance(end+61*Second); CHECK(r.t().seconds==61 && r.screens.vibration()==0);
    // Held at 99:59:59.
    r.advance(end+TimeUs(TimerMaxSeconds+5)*Second);
    CHECK(r.t().seconds==TimerMaxSeconds && r.screens.nextUpdate()==INT64_MAX);
    // The button dismisses; the setup returns with the last length.
    r.tap(timerDismissBox(r.m()));
    CHECK(r.screens.timer.state()==TimerState::Idle && r.t().view==TimerView::Setup && r.t().fields[1]==3);
    CHECK(r.screens.holdPanelUntil()==0 && r.screens.vibration()==0);
    // So does B; and leaving for another screen does too, but a hidden
    // screen left again later dismisses nothing.
    r.tap(timerSetBox(r.m()));
    r.screens.timer.expire(r.screens.timer.deadline());
    r.screens.present(ScreenId::Timer,r.now);
    r.b(); CHECK(r.screens.timer.state()==TimerState::Idle);
    r.tap(timerSetBox(r.m()));
    r.screens.timer.expire(r.screens.timer.deadline());
    r.screens.present(ScreenId::Timer,r.now);
    CHECK(r.screens.present(ScreenId::Stopwatch,r.now) && r.screens.timer.state()==TimerState::Idle);
    r.screens.timer.start(r.now,1); r.screens.timer.expire(r.now+Second);
    CHECK(r.screens.present(ScreenId::Home,r.now+Second) && r.screens.timer.state()==TimerState::Ringing);
}
}
int main() {
    normalizes(); countsDown(); ignoresTheDate(); plansAlignments(); takesCorrections(); vibrates(); remembers();
    alertsWhileDark(); alignsWhileDark(); replansOnWake(); showsTheCorrection();
    dismissEndsTheAlert(); retriesTheMotor(); expiryBeforeInput(); externalBoot();
    attentionContract(); presenting();
    setsUp(); setupEdges(); countsDownOnScreen(); resetsByTouch(); ringsOnScreen();
    std::cout << "PASS: normalize, countdown, date independence, alignment plan, clock corrections, "
                 "vibration pattern, last length record, alert while dark, alignment while dark, "
                 "replan on wake, corrected countdown, dismiss, motor retry, expiry before input, external boot, "
                 "attention contract, presenting, setup, setup edges, countdown screen, touch reset, ringing screen\n";
}
