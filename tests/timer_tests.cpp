#include "host/HostApplication.h"
#include "TestScreens.h"
#include "host/LaunchRegistry.h"
#include "features/timer/TimerVibration.h"
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
constexpr TimeUs Second=1000000;
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
        if (motorWrites) motor=level;
        return motorWrites;
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
void vibrates() {
    // Weak and sparse, then denser, then strong, for one minute.
    auto at=[](double seconds) { return timerVibration(TimeUs(seconds*Second)); };
    CHECK(at(0).level==VibrationWeak && at(0).until==200000);
    CHECK(at(0.2).level==0 && at(0.2).until==2*Second);
    CHECK(at(9.9).level==0 && at(9.9).until==10*Second);
    CHECK(at(10).level==VibrationMedium && at(10).until==10200000);
    CHECK(at(10.3).level==0 && at(10.3).until==10500000);
    CHECK(at(10.5).level==VibrationMedium && at(10.5).until==10700000);
    CHECK(at(10.7).level==0 && at(10.7).until==12*Second);
    CHECK(at(20).level==VibrationStrong && at(20).until==20400000);
    CHECK(at(20.4).level==0 && at(20.4).until==20600000);
    CHECK(at(59.9).level==VibrationStrong && at(59.9).until==60*Second); // Cut at the end.
    CHECK(at(60).level==0 && at(60).until==INT64_MAX);
    CHECK(at(-1).level==VibrationWeak);
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

// The runtime around a running timer: built like the device's.
struct Rig {
    FakeHal hal;
    HostApplication app{hal,hal,hal,468,468};
    HostRuntime& runtime=app.runtime();
    TimerService& timer=app.timer();
    Rig() { runtime.begin(); runtime.step(); }
    void at(TimeUs t) { hal.time=t; runtime.step(); }
    // Steps at the input period over [from, to], as a held button is followed.
    void follow(TimeUs from,TimeUs to) { for (TimeUs t=from;t<=to;t+=10000) at(t); }
};
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
    CHECK(r.timer.state()==TimerState::Ringing && r.app.timerAttention().alerting(r.hal.time));
    CHECK(!r.runtime.power().screenOff() && r.hal.wakes==1 && !r.hal.lightSleep);
    CHECK(r.hal.motor==VibrationWeak);
    // The motor follows its pattern on the runtime's own deadlines.
    // The panel's fade wakes the loop as well, so the motor is followed by step.
    r.runtime.wait(); CHECK(r.hal.time<=end+200000);
    r.at(end+200000-1); CHECK(r.hal.motor==VibrationWeak);
    r.at(end+200000); CHECK(r.hal.motor==0);
    r.at(end+2*Second-1); CHECK(r.hal.motor==0);
    r.at(end+2*Second); CHECK(r.hal.motor==VibrationWeak);
    // Lit for the whole minute, rests included, though nothing is pressed.
    for (TimeUs t=end+3*Second;t<end+60*Second;t+=3*Second) { r.at(t); CHECK(!r.runtime.power().screenOff()); }
    r.at(end+60*Second);
    CHECK(r.hal.motor==0 && !r.app.timerAttention().alerting(r.hal.time) && r.timer.state()==TimerState::Ringing);
    // Then the usual timeout, counted from the end of the alert.
    r.at(end+90*Second-1); CHECK(!r.runtime.power().screenOff());
    r.at(end+90*Second); CHECK(r.runtime.power().screenOff() && r.hal.sleeps==2);
    // Still ringing: nothing restarts the alert.
    const auto writes=r.hal.levels.size();
    r.at(end+200*Second); CHECK(r.hal.levels.size()==writes && r.runtime.power().screenOff());
}
// Dismissed early, the motor stops and the timeout runs from the input that
// dismissed it, not from the alert's end.
void dismissEndsTheAlert() {
    Rig r;
    CHECK(r.timer.start(r.hal.time,5));
    r.at(5*Second);
    CHECK(r.app.timerAttention().alerting(r.hal.time) && r.hal.motor==VibrationWeak);
    r.hal.input.a=true; r.at(10*Second);
    r.hal.input.a=false; r.timer.dismiss(); r.at(10*Second+10000);
    CHECK(!r.app.timerAttention().alerting(r.hal.time) && r.hal.motor==0);
    r.at(40*Second); CHECK(!r.runtime.power().screenOff());
    r.at(40*Second+10000); CHECK(r.runtime.power().screenOff());
}
// A write that does not land is sent again, during the alert and after it.
void retriesTheMotor() {
    Rig r;
    CHECK(r.timer.start(r.hal.time,5));
    r.hal.motorWrites=false;
    r.at(5*Second);
    CHECK(r.hal.motor==0 && r.hal.levels.size()==1);
    r.runtime.wait(); CHECK(r.hal.time==5*Second+50000);
    r.hal.motorWrites=true; r.runtime.step();
    CHECK(r.hal.motor==VibrationWeak);
    // Stopping fails at first too: it is retried after the timer is gone.
    r.hal.motorWrites=false;
    r.timer.dismiss(); r.at(5*Second+100000);
    CHECK(r.hal.levels.back()==0 && r.hal.motor==VibrationWeak);
    r.runtime.wait(); CHECK(r.hal.time==5*Second+150000);
    r.hal.motorWrites=true; r.runtime.step();
    CHECK(r.hal.motor==0);
    const auto writes=r.hal.levels.size();           // Nothing left to retry.
    r.at(r.hal.time+Second); r.at(r.hal.time+Second);
    CHECK(r.hal.levels.size()==writes);
}
// The expiry is settled before the input of the same step: a release that
// arrives with it reaches nothing (docs/task12/plan.md 2.4).
void expiryBeforeInput() {
    // B released in the step the timer runs out, on the list: no launch.
    {
        Rig r;
        r.hal.input.a=true; r.at(10000);
        r.hal.input.a=false; r.at(20000);
        r.at(400000);
        CHECK(r.runtime.model().screen==ScreenId::AppList);
        CHECK(r.timer.start(r.hal.time,2));
        const TimeUs end=r.timer.deadline();
        r.hal.input.b=true; r.follow(end-300000,end-10000);
        r.hal.input.b=false; r.at(end);
        CHECK(r.timer.state()==TimerState::Ringing);
        CHECK(r.runtime.model().screen==ScreenId::AppList);
        // The next press is new and works.
        r.hal.input.a=true; r.at(end+100000);
        r.hal.input.a=false; r.at(end+110000);
        r.at(end+400000);
        CHECK(r.runtime.model().launcher.list.selection==1);
    }
    // A+B held across the expiry: no home from that hold.
    {
        Rig r;
        r.hal.input.a=true; r.at(10000);
        r.hal.input.a=false; r.at(20000);
        r.at(400000);
        CHECK(r.timer.start(r.hal.time,1));
        const TimeUs end=r.timer.deadline();
        r.hal.input.a=r.hal.input.b=true; r.follow(end-200000,end+1000000);
        CHECK(r.runtime.model().screen==ScreenId::AppList && r.runtime.model().homeCount==0);
        r.hal.input.a=r.hal.input.b=false; r.at(end+1010000);
        // Pressed again, it is home.
        r.hal.input.a=r.hal.input.b=true; r.follow(end+1100000,end+1800000);
        CHECK(r.runtime.model().screen==ScreenId::Home && r.runtime.model().homeCount==1);
    }
    // A paused press does not land on a timer that already ran out: the
    // service refuses, whatever the caller.
    {
        TimerService t; t.start(0,1);
        CHECK(!t.pause(Second) && t.expire(Second));
    }
}
// An external boot ends the timer and the motor, ringing or not; a boot that
// fails leaves both alone (docs/task12/plan.md 1, 2.4).
void externalBoot() {
    for (bool succeeds:{true,false}) {
        Rig r;
        FakeSlotService slots; slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
        slots.bootSucceeds=succeeds;
        r.app.bindSlots(slots);
        CHECK(r.timer.start(r.hal.time,1));
        r.at(Second);
        CHECK(r.timer.state()==TimerState::Ringing && r.hal.motor==VibrationWeak);
        auto press=[&](bool a) {
            (a ? r.hal.input.a : r.hal.input.b)=true; r.at(r.hal.time+20000);
            (a ? r.hal.input.a : r.hal.input.b)=false; r.at(r.hal.time+20000);
            r.at(r.hal.time+200000);
        };
        press(true);
        while (LaunchRegistry[r.runtime.model().launcher.list.selection].id!=LaunchTargetId::External1) press(true);
        press(false);
        CHECK(slots.bootRequests==1);
        if (succeeds) CHECK(r.timer.state()==TimerState::Idle && r.hal.levels.back()==0);
        else CHECK(r.timer.state()==TimerState::Ringing && r.app.timerAttention().alerting(r.hal.time));
    }
    // A running timer is stopped as well.
    TimerService timer; StopwatchService stopwatch; FakeHal hal;
    HostShutdown shutdown(stopwatch,timer,hal);
    timer.start(0,60);
    shutdown.onBootCommitted();
    CHECK(timer.state()==TimerState::Idle && hal.levels.size()==1 && hal.levels[0]==0);
}
}
// The host side alone, with sources of the test's own: what any feature that
// asks for attention gets (host/AttentionSource.h).
struct FakeSource : AttentionSource {
    AttentionRequest request{};
    TimeUs started=-1, next=INT64_MAX;
    int asked=0, starts=0;
    AttentionRequest attention(TimeUs) override { ++asked; return request; }
    void attended(TimeUs now) override { started=now; ++starts; }
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
    // lights, the screen is presented and the source learns when.
    r.hal.input.b=true; r.at(99*Second);
    source.request.active=true; source.request.present=true; source.request.screen=ScreenId::Stopwatch;
    source.request.holdUntil=200*Second; source.request.vibration=77;
    source.next=INT64_MAX;
    r.hal.input.b=false; r.at(100*Second);
    CHECK(source.starts==1 && source.started==100*Second);
    CHECK(!r.runtime.power().screenOff() && r.runtime.model().screen==ScreenId::Stopwatch);
    CHECK(r.app.stopwatch().state()==StopwatchState::Reset); // The release started nothing.
    CHECK(r.hal.motor==77);
    // Started once; the hold and the motor follow what it asks now.
    source.request.vibration=0; r.at(101*Second);
    CHECK(source.starts==1 && r.hal.motor==0);
    r.at(229*Second); CHECK(!r.runtime.power().screenOff()); // Held, then the timeout.
    r.at(230*Second); CHECK(r.runtime.power().screenOff());
    // Ending the request lets the panel time out from the last input, and a
    // new request starts again.
    source.request.active=false; r.at(231*Second);
    source.request={}; source.request.active=true; source.request.vibration=50;
    r.at(232*Second);
    CHECK(source.starts==2 && !r.runtime.power().screenOff() && r.hal.motor==50);
    source.request.active=false;
    r.hal.input.a=true; r.at(233*Second); r.hal.input.a=false; r.at(233*Second+10000);
    CHECK(r.hal.motor==0);
    r.at(263*Second); CHECK(!r.runtime.power().screenOff());
    r.at(263*Second+10000); CHECK(r.runtime.power().screenOff());
    // Two at once: the stronger motor and the later hold win.
    FakeSource other;
    CHECK(r.runtime.bindAttention(other));
    source.request={}; source.request.active=true; source.request.vibration=40; source.request.holdUntil=300*Second;
    other.request={}; other.request.active=true; other.request.vibration=90; other.request.holdUntil=280*Second;
    r.at(270*Second);
    CHECK(r.hal.motor==90);
    other.request.active=false; r.at(271*Second);
    CHECK(r.hal.motor==40);
    r.at(329*Second); CHECK(!r.runtime.power().screenOff());
    // Capacity: four sources in all, the timer's among them.
    FakeSource third, fourth;
    CHECK(r.runtime.bindAttention(third) && !r.runtime.bindAttention(fourth));
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
}
int main() {
    normalizes(); countsDown(); ignoresTheDate(); vibrates(); remembers();
    alertsWhileDark(); dismissEndsTheAlert(); retriesTheMotor(); expiryBeforeInput(); externalBoot();
    attentionContract(); presenting();
    std::cout << "PASS: normalize, countdown, date independence, vibration pattern, last length record, "
                 "alert while dark, dismiss, motor retry, expiry before input, external boot, "
                 "attention contract, presenting\n";
}
