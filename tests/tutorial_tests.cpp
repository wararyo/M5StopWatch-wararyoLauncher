#include "host/HostApplication.h"
#include "TestScreens.h"
#include "features/settings/SettingsLayout.h"
#include "features/tutorial/TutorialScreen.h"
#include "i18n/Strings.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
struct MemoryRecords : RecordBackend {
    std::map<std::string,std::vector<uint8_t>> records;
    bool readable=true,writable=true;
    int writes=0;
    PrefResult read(const char* key,uint8_t* data,size_t& size) override {
        if (!readable) return PrefResult::Unavailable;
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
// An RTC that holds the time, or one never set (year 2000 reads as unset),
// and one that takes no writes.
struct StubHal : Hal {
    CivilTime rtc{2000,1,1,0,0,0};
    bool rtcWritable=true;
    int64_t clockUs=0;
    TimeUs time=0;
    InputSnapshot input{};
    TimeUs now() override { return time; }
    InputSnapshot sampleInput() override { return input; }
    UsbState sampleUsb() override { return {}; }
    void waitUs(TimeUs) override {}
    bool inputPending() override { return input.a || input.b || input.touching; }
    bool readRtc(CivilTime& utc) override { utc=rtc; return true; }
    bool writeRtc(const CivilTime& utc) override { if (!rtcWritable) return false; rtc=utc; return true; }
    void setUtcClock(int64_t seconds) override { clockUs=seconds*1000000; }
    int64_t utcClockUs() override { return clockUs; }
    BatteryState sampleBattery() override { return {}; }
    void setScreenOff(bool) override {}
    void setBrightness(int) override {}
};
struct StubRender : RenderPort {
    void invalidate() override {}
    void draw(const FrameModel&,const WatchData&) override {}
    TimeUs nextUpdate(TimeUs,const WatchData&) const override { return INT64_MAX; }
};
Events tap(int x,int y) { Events e{}; e.gesture=Gesture::Tap; e.x=x; e.y=y; return e; }
Events press(bool next) { Events e{}; e.next=next; e.decide=!next; return e; }
Events touchEvent(Gesture g,int y,int x=234) { Events e{}; e.gesture=g; e.x=x; e.y=y; return e; }
Events homeEvent() { Events e{}; e.home=true; e.chordChanged=true; return e; }
const SettingsGeometry DateGeometry{{468,468},SettingsView::DateTime,0};
Rect button(int index) { return settingsButtonBox(DateGeometry,index); }
// A swipe from the top edge let go far enough down (docs/task14/plan.md 2.1).
bool swipeHome(ScreenManager& s,TimeUs& now) {
    s.handle(touchEvent(Gesture::TouchStart,20),now+=10000);
    s.handle(touchEvent(Gesture::DragStart,90),now+=10000);
    s.handle(touchEvent(Gesture::DragMove,200),now+=10000);
    const bool changed=s.handle(touchEvent(Gesture::DragEnd,200),now+=10000);
    s.update(now+=HomeRevealUs);
    return changed;
}
void bindTutorial(TestScreens& s,TimeService* time,TutorialRecord* record) { s.bind(nullptr,time); s.bindTutorial(record); }
bool finished(const MemoryRecords& records) {
    const auto found=records.records.find(TutorialRecord::Key);
    return found!=records.records.end() && found->second==std::vector<uint8_t>{TutorialRecord::Format};
}
}
// docs/task15/plan.md 1.4, 2.4: only a missing record brings it up.
void record() {
    MemoryRecords records; TutorialRecord record;
    CHECK(record.load()==PrefResult::Unavailable && !record.due()); // Unbound: the render check.
    record.bind(&records);
    CHECK(record.load()==PrefResult::Missing && record.due());
    CHECK(record.complete()==PrefResult::Ok && !record.due() && records.writes==1 && finished(records));
    CHECK(record.load()==PrefResult::Ok && !record.due());
    records.records[TutorialRecord::Key]={2};          // A format this build does not know.
    CHECK(record.load()==PrefResult::Invalid && !record.due());
    records.records[TutorialRecord::Key]={1,0};        // The wrong length.
    CHECK(record.load()==PrefResult::Invalid && !record.due());
    records.readable=false;                            // Storage that cannot be read.
    CHECK(record.load()==PrefResult::Unavailable && !record.due());
    records.readable=true; records.records.clear(); records.writable=false;
    CHECK(record.load()==PrefResult::Missing && record.due());
    CHECK(record.complete()==PrefResult::WriteFailed && record.due());
    // A face can never take the key.
    CHECK(reservedRecordKey(TutorialRecord::Key));
    WatchPreferences preferences; preferences.bind(&records);
    const uint8_t payload[1]{};
    CHECK(preferences.saveFace(TutorialRecord::Key,payload,sizeof(payload))==PrefResult::Invalid);
}
// docs/task15/plan.md 1.1, 1.2: the date while the clock is unset, then the way home.
void steps() {
    StubHal hal; MemoryRecords records; TutorialRecord record; record.bind(&records); record.load();
    TimeService time; time.begin(hal);
    CHECK(!time.valid());
    TestScreens s; bindTutorial(s,&time,&record);
    TimeUs now=0;
    CHECK(s.present(ScreenId::Tutorial,now) && s.model().screen==ScreenId::Tutorial);
    auto model=s.model().tutorial;
    CHECK(model.step==TutorialStep::DateTime && model.editor.view==SettingsView::DateTime && model.editor.cursor==0);
    CHECK(model.editor.fields[0]==TimeService::MinYear && model.editor.fields[1]==1 && model.editor.fields[2]==1);
    // B steps the focused field, held B repeats, as in settings.
    s.handle(press(false),now+=10000);
    CHECK(s.model().tutorial.editor.fields[0]==TimeService::MinYear+1);
    Events hold{}; hold.hold=Hold::B; hold.holdChanged=true; hold.holdSince=now+=10000;
    CHECK(s.handle(hold,now));
    CHECK(s.model().tutorial.editor.fields[0]==TimeService::MinYear+2 && s.nextUpdate()==now+HoldRepeat::DelayUs);
    Events up{}; up.holdChanged=true; up.decide=true; up.pressUs=10000;
    s.handle(up,now+=10000);
    CHECK(s.model().tutorial.editor.fields[0]==TimeService::MinYear+2 && s.nextUpdate()==INT64_MAX);
    // An impossible date stays, with the notice settings shows.
    s.handle(press(true),now+=10000);   // The month,
    s.handle(press(false),now+=10000);  // February,
    const Rect dayUp=settingsArrowBox(DateGeometry,2,true),dayDown=settingsArrowBox(DateGeometry,2,false);
    s.handle(tap(dayDown.x+dayDown.w/2,dayDown.y+dayDown.h/2),now+=10000); // the 31st.
    CHECK(s.model().tutorial.editor.fields[1]==2 && s.model().tutorial.editor.fields[2]==31);
    const Rect save=button(0);
    s.handle(tap(save.x+save.w/2,save.y+save.h/2),now+=10000);
    CHECK(s.model().tutorial.step==TutorialStep::DateTime && !time.valid());
    CHECK(s.model().toast && std::strcmp(s.model().toast,text::InvalidDate)==0);
    // A real date: saved, and on to the way home.
    s.handle(tap(dayUp.x+dayUp.w/2,dayUp.y+dayUp.h/2),now+=10000);       // The 1st.
    s.handle(tap(save.x+save.w/2,save.y+save.h/2),now+=10000);
    CHECK(s.model().tutorial.step==TutorialStep::Guide && time.valid());
    CHECK(s.model().toast && std::strcmp(s.model().toast,text::TimeSaved)==0);
    CHECK(hal.rtc.year==TimeService::MinYear+2 && hal.rtc.month==1 && hal.rtc.day==31 && hal.rtc.hour==15); // 1 Feb 00:00 JST.
    // The way home has nothing to press.
    CHECK(!s.handle(tap(234,234),now+=10000));
    CHECK(!s.handle(press(true),now+=10000) && !s.handle(press(false),now+=10000));
    Events b{}; b.hold=Hold::B; b.holdChanged=true; b.holdSince=now+=10000;
    CHECK(!s.handle(b,now));
    CHECK(s.model().screen==ScreenId::Tutorial && s.model().tutorial.step==TutorialStep::Guide);
    CHECK(s.update(now+=2000000)); // Only the notice goes.
    CHECK(s.nextUpdate()==INT64_MAX && records.writes==0);
    // Cancel goes on with the clock still unset.
    StubHal unset; TimeService other; other.begin(unset);
    TestScreens t; bindTutorial(t,&other,&record);
    t.present(ScreenId::Tutorial,0);
    t.handle(tap(button(1).x+button(1).w/2,button(1).y+button(1).h/2),10000);
    CHECK(t.model().tutorial.step==TutorialStep::Guide && !other.valid() && t.model().toast==nullptr);
    // A clock already set, or none at all: straight to the way home.
    TestScreens u; bindTutorial(u,&time,&record);
    CHECK(u.present(ScreenId::Tutorial,0) && u.model().tutorial.step==TutorialStep::Guide);
    TestScreens v; bindTutorial(v,nullptr,&record);
    CHECK(v.present(ScreenId::Tutorial,0) && v.model().tutorial.step==TutorialStep::Guide);
    // Without the record it cannot be finished, so it does not open.
    TestScreens w; bindTutorial(w,&time,nullptr);
    CHECK(!w.present(ScreenId::Tutorial,0) && w.model().screen==ScreenId::Home);
}
// docs/task15/plan.md 1.2: only a saved date goes on to the way home. A
// write that fails keeps the entry where it was, to be saved again.
void saveFailure() {
    StubHal hal; hal.rtcWritable=false; MemoryRecords records; TutorialRecord record; record.bind(&records); record.load();
    TimeService time; time.begin(hal);
    TestScreens s; bindTutorial(s,&time,&record);
    TimeUs now=0;
    s.present(ScreenId::Tutorial,now);
    s.handle(press(false),now+=10000);  // The year,
    s.handle(press(true),now+=10000);   // on to the month,
    s.handle(press(false),now+=10000);  // February.
    const auto entered=s.model().tutorial.editor;
    const Rect save=button(0);
    s.handle(tap(save.x+save.w/2,save.y+save.h/2),now+=10000);
    auto m=s.model();
    CHECK(m.screen==ScreenId::Tutorial && m.tutorial.step==TutorialStep::DateTime);
    CHECK(m.tutorial.editor.cursor==5); // Save, which the tap focused, as in settings.
    CHECK(std::equal(std::begin(entered.fields),std::end(entered.fields),std::begin(m.tutorial.editor.fields)));
    // The RTC never held a time, so there is nothing to fall back on.
    CHECK(m.toast && std::strcmp(m.toast,text::ClockUnavailable)==0);
    CHECK(!time.valid() && hal.rtc.year==2000 && records.writes==0 && record.due());
    // B saves again once the RTC takes it.
    hal.rtcWritable=true;
    s.handle(press(false),now+=10000);
    m=s.model();
    CHECK(m.tutorial.step==TutorialStep::Guide && time.valid());
    CHECK(m.toast && std::strcmp(m.toast,text::TimeSaved)==0);
    CHECK(hal.rtc.year==TimeService::MinYear+1 && hal.rtc.month==1 && hal.rtc.day==31 && records.writes==0);
}
// The panel going dark leaves the tutorial as it was, through the runtime:
// the step, the entry and the focus. The touch that wakes it is spent, so it
// presses nothing (docs/architecture.md, input and waking).
void screenOff() {
    StubHal hal; StubRender render; DisplayDataSource data; TimeService time; time.begin(hal);
    SettingsStore store; MemoryRecords records;
    HostApplication application(hal,render,data,468,468);
    application.bindSettings(store,time);
    application.bindRecords(records);
    application.begin();
    auto& runtime=application.runtime();
    const auto tutorial=[&] { return application.screens().model().tutorial; };
    const auto click=[&](bool next) {
        hal.input={next,!next,false,0,0}; hal.time+=10000; runtime.step();
        hal.input={}; hal.time+=10000; runtime.step();
    };
    const auto touch=[&](Rect box) {
        hal.input={false,false,true,box.x+box.w/2,box.y+box.h/2}; hal.time+=10000; runtime.step();
        hal.input={}; hal.time+=10000; runtime.step();
    };
    const auto sleep=[&] {
        for (int i=0;i<31 && !runtime.power().screenOff();++i) { hal.time+=1000000; runtime.step(); }
        return runtime.power().screenOff();
    };
    runtime.step();
    click(false);                       // B: the year,
    click(true);                        // A: on to the month,
    click(false);                       // B: February.
    const auto entered=tutorial().editor;
    CHECK(tutorial().step==TutorialStep::DateTime && entered.cursor==1 && entered.fields[1]==2);
    // Dark, then woken by a touch on Save: nothing saved, nothing moved.
    CHECK(sleep());
    touch(button(0));
    CHECK(!runtime.power().screenOff());
    auto m=application.screens().model();
    CHECK(m.screen==ScreenId::Tutorial && m.tutorial.step==TutorialStep::DateTime && m.toast==nullptr);
    CHECK(m.tutorial.editor.cursor==entered.cursor);
    CHECK(std::equal(std::begin(entered.fields),std::end(entered.fields),std::begin(m.tutorial.editor.fields)));
    CHECK(!time.valid() && hal.rtc.year==2000);
    // Awake, the same touch saves, and the way home follows.
    touch(button(0));
    CHECK(tutorial().step==TutorialStep::Guide && time.valid());
    // The way home survives the dark too, and waking does not finish it.
    CHECK(sleep());
    touch({0,0,468,40});
    CHECK(!runtime.power().screenOff() && application.screens().model().screen==ScreenId::Tutorial);
    // (The pedometer saves its day as the panel goes dark, so only the key tells.)
    CHECK(tutorial().step==TutorialStep::Guide && records.records.count(TutorialRecord::Key)==0);
    // A+B, held, still finishes it.
    hal.input={true,true,false,0,0}; hal.time+=10000; runtime.step();
    hal.time+=600000; runtime.step();
    CHECK(application.screens().model().screen==ScreenId::Home && finished(records));
}
// docs/task15/plan.md 1.3, 2.3: finished only by going home from the way home.
void finishing() {
    StubHal hal; MemoryRecords records; TutorialRecord record; record.bind(&records); record.load();
    TimeService time; time.begin(hal);
    TestScreens s; bindTutorial(s,&time,&record);
    TimeUs now=0;
    // Home from the date step: home, but not finished.
    s.present(ScreenId::Tutorial,now);
    CHECK(s.model().tutorial.step==TutorialStep::DateTime);
    CHECK(swipeHome(s,now) && s.model().screen==ScreenId::Home && s.model().homeCount==1);
    CHECK(records.writes==0 && record.due());
    s.present(ScreenId::Tutorial,now+=10000);
    CHECK(s.handle(homeEvent(),now+=10000) && s.model().screen==ScreenId::Home);
    s.update(now+=HomeRevealUs);
    CHECK(records.writes==0 && record.due());
    // Brought aside by an attention request: not finished either.
    s.present(ScreenId::Tutorial,now+=10000);
    s.handle(tap(button(1).x+button(1).w/2,button(1).y+button(1).h/2),now+=10000);
    CHECK(s.model().tutorial.step==TutorialStep::Guide);
    CHECK(s.present(ScreenId::Timer,now+=10000) && s.model().screen==ScreenId::Timer);
    CHECK(records.writes==0 && record.due());
    // Opened again, it starts from the date: the clock is still unset.
    s.present(ScreenId::Tutorial,now+=10000);
    CHECK(s.model().tutorial.step==TutorialStep::DateTime);
    // From the way home, the swipe finishes it, the clock coming in as usual.
    s.handle(tap(button(1).x+button(1).w/2,button(1).y+button(1).h/2),now+=10000);
    s.handle(touchEvent(Gesture::TouchStart,20),now+=10000);
    s.handle(touchEvent(Gesture::DragStart,90),now+=10000);
    s.handle(touchEvent(Gesture::DragMove,200),now+=10000);
    CHECK(s.handle(touchEvent(Gesture::DragEnd,200),now+=10000));
    CHECK(s.model().screen==ScreenId::Home && s.revealing());
    CHECK(records.writes==1 && finished(records) && !record.due());
    s.update(now+=HomeRevealUs);
    // A+B from the way home finishes it too.
    MemoryRecords others; TutorialRecord second; second.bind(&others); second.load();
    TestScreens t; bindTutorial(t,nullptr,&second);
    t.present(ScreenId::Tutorial,0);
    CHECK(t.handle(homeEvent(),10000) && t.model().screen==ScreenId::Home && t.model().homeGesture.revealing);
    CHECK(others.writes==1 && finished(others));
    // A failed write leaves it due, with nothing shown: the next boot asks again.
    MemoryRecords failing; failing.writable=false; TutorialRecord third; third.bind(&failing); third.load();
    TestScreens u; bindTutorial(u,nullptr,&third);
    u.present(ScreenId::Tutorial,0);
    u.handle(homeEvent(),10000);
    CHECK(failing.writes==1 && third.due() && u.model().toast==nullptr && u.model().screen==ScreenId::Home);
    // Home on other screens never asks anything of the tutorial.
    MemoryRecords quiet; TutorialRecord fourth; fourth.bind(&quiet); fourth.load();
    TestScreens v; bindTutorial(v,nullptr,&fourth);
    v.present(ScreenId::Timer,0);
    v.handle(homeEvent(),10000);
    CHECK(quiet.writes==0 && fourth.due());
}
// docs/task15/plan.md 2.5: the host opens it at start when the record is missing.
void startup() {
    StubHal hal; StubRender render; DisplayDataSource data; TimeService time; time.begin(hal);
    SettingsStore store;
    {
        MemoryRecords records;
        HostApplication application(hal,render,data,468,468);
        application.bindSettings(store,time);
        application.bindRecords(records);
        application.begin();
        CHECK(application.screens().model().screen==ScreenId::Tutorial);
        CHECK(application.screens().model().tutorial.step==TutorialStep::DateTime);
        application.runtime().step();
        CHECK(application.screens().model().screen==ScreenId::Tutorial);
    }
    {
        MemoryRecords records; records.records[TutorialRecord::Key]={TutorialRecord::Format};
        HostApplication application(hal,render,data,468,468);
        application.bindRecords(records);
        application.begin();
        CHECK(application.screens().model().screen==ScreenId::Home);
    }
    {
        // No records bound (the render check): never.
        HostApplication application(hal,render,data,468,468);
        application.begin();
        CHECK(application.screens().model().screen==ScreenId::Home);
    }
}
int main() {
    record(); steps(); saveFailure(); screenOff(); finishing(); startup();
    std::cout << "PASS: tutorial record, steps, save failure, screen off, finishing, startup\n";
}
