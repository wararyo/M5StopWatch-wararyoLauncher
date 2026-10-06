#include "host/HostApplication.h"
#include "TestScreens.h"
#include "host/LaunchRegistry.h"
#include "features/settings/SettingsMenu.h"
#include "i18n/Strings.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
// Labels as this build's language spells them (i18n/Strings.h).
std::string menuLabel(const char* name,const std::string& value) { return std::string(name)+"  "+value; }
std::string seconds(int value) {
    char buffer[16]; std::snprintf(buffer,sizeof(buffer),text::SecondsFormat,value);
    return buffer;
}
struct MemoryBackend : SettingsBackend {
    uint8_t blob[8]{};
    size_t stored=0;
    bool present=false,writable=true;
    int writes=0;
    bool load(void* data,size_t& size) override {
        if (!present || size<stored) return false;
        std::memcpy(data,blob,stored); size=stored; return true;
    }
    bool save(const void* data,size_t size) override {
        ++writes;
        if (!writable) return false;
        std::memcpy(blob,data,size); stored=size; present=true; return true;
    }
};
struct StubHal : Hal {
    CivilTime rtc{2026,9,20,15,0,0};
    bool rtcWritable=true;
    int brightness=-1,brightnessCalls=0;
    int64_t clockUs=0;
    TimeUs time=0,waited=-1;
    InputSnapshot input{};
    TimeUs now() override { return time; }
    InputSnapshot sampleInput() override { return input; }
    int usbPolls=0;
    UsbState sampleUsb() override { ++usbPolls; return {}; }
    // A woken panel shows its image this long after the wake.
    TimeUs showsAfter=0,showsAt=0;
    void setScreenOff(bool off) override { if(!off) showsAt=time+showsAfter; }
    TimeUs panelShowsAt() const override { return showsAt; }
    void waitUs(TimeUs us) override { waited=us; }
    // A low-level interrupt: pending for as long as anything is pressed.
    bool inputPending() override { return input.a || input.b || input.touching; }
    bool readRtc(CivilTime& utc) override { utc=rtc; return true; }
    bool writeRtc(const CivilTime& utc) override { if(!rtcWritable) return false; rtc=utc; return true; }
    void setUtcClock(int64_t seconds) override { clockUs=seconds*1000000; }
    int64_t utcClockUs() override { return clockUs; }
    BatteryState sampleBattery() override { return {}; }
    void setBrightness(int level) override { brightness=level; ++brightnessCalls; }
};
struct StubRender : RenderPort {
    int draws=0;
    void invalidate() override {}
    void draw(const FrameModel&,const WatchData&) override { ++draws; }
    TimeUs nextUpdate(TimeUs,const WatchData&) const override { return INT64_MAX; }
};
Events tap(int x,int y) { Events e{}; e.gesture=Gesture::Tap; e.x=x; e.y=y; return e; }
Events press(bool next) { Events e{}; e.next=next; e.decide=!next; return e; }
// Drives the manager straight to the settings menu the way the list does.
// Starts from home, so it works wherever the previous case left off.
void openSettings(ScreenManager& s,TimeUs& now) {
    Events home{}; home.home=true; s.handle(home,now); now+=1000;
    Events e{}; e.next=true;
    s.handle(e,now); now+=200000; s.update(now);      // clock -> list
    while (LaunchRegistry[s.model().launcher.list.selection].id!=LaunchTargetId::Settings) { s.handle(e,now); now+=200000; s.update(now); }
    Events decide{}; decide.decide=true;
    s.handle(decide,now); now+=1000;
    CHECK(s.model().screen==ScreenId::Settings);
}
}
void storeRecord() {
    MemoryBackend backend; SettingsStore store;
    CHECK(!store.begin(backend)); // Nothing stored yet: defaults, no write.
    CHECK(store.get().brightness==90 && store.get().screenOffSec==30 && backend.writes==0);
    CHECK(store.save({120,60}) && backend.writes==1);
    SettingsStore reopened;
    CHECK(reopened.begin(backend));
    CHECK(reopened.get().brightness==120 && reopened.get().screenOffSec==60);
    // Out of range values are refused before anything is written.
    CHECK(!store.save({10,60}) && !store.save({120,45}) && backend.writes==1);
    CHECK(store.get().brightness==120); // The refusal did not disturb RAM.
    // A failed write leaves RAM matching what survives a reboot.
    backend.writable=false;
    CHECK(!store.save({45,15}) && store.get().brightness==120);
    backend.writable=true;
    // An unknown schema is diagnosed and left alone, never rewritten.
    MemoryBackend future; future.present=true; future.stored=5;
    SettingsStore::encode({200,15},future.blob);
    future.blob[0]=99;
    SettingsStore ahead;
    CHECK(!ahead.begin(future) && ahead.get().brightness==90 && future.writes==0);
    // One bad field keeps the other.
    MemoryBackend mixed; mixed.present=true; mixed.stored=5;
    SettingsStore::encode({200,15},mixed.blob);
    mixed.blob[2]=5; // Below the readable minimum.
    SettingsStore partial;
    CHECK(!partial.begin(mixed));
    CHECK(partial.get().brightness==90 && partial.get().screenOffSec==15);
    // A truncated record is unusable, not half-applied.
    MemoryBackend short_; short_.present=true; short_.stored=3;
    SettingsStore truncated;
    CHECK(!truncated.begin(short_) && truncated.get().brightness==90);
}
void menuAndEditors() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    screens.setInfo("wararyoLauncher","0.1.0","5.5.0");
    TimeUs now=0;
    openSettings(screens,now);
    CHECK(screens.model().settings.view==SettingsView::Menu);
    // A cycles the six rows and wraps.
    for (int i=0;i<5;++i) screens.handle(press(true),now);
    CHECK(screens.model().settings.menu.selection==5);
    screens.handle(press(true),now);
    CHECK(screens.model().settings.menu.selection==0);
    // The last row leaves back to the list, keeping its selection.
    const int listRow=screens.model().launcher.list.selection;
    for (int i=0;i<5;++i) screens.handle(press(true),now);
    screens.handle(press(false),now);
    CHECK(screens.model().screen==ScreenId::AppList && screens.model().launcher.list.selection==listRow);
    openSettings(screens,now);
    // Row 0 opens the date editor, seeded with the current time.
    screens.handle(press(false),now);
    const auto& s=screens.model().settings;
    CHECK(s.view==SettingsView::DateTime && s.cursor==0);
    CHECK(s.fields[0]==2026 && s.fields[1]==9 && s.fields[2]==21 && s.fields[3]==0 && s.fields[4]==0);
    // B steps the focused field and A moves the focus on (docs/task12/plan.md 1.5).
    screens.handle(press(false),now);
    CHECK(screens.model().settings.fields[0]==2027 && screens.model().settings.cursor==0);
    screens.handle(press(true),now);
    CHECK(screens.model().settings.cursor==1 && screens.model().settings.fields[0]==2027);
    screens.handle(press(false),now);
    CHECK(screens.model().settings.fields[1]==10 && screens.model().settings.cursor==1);
    // The year wraps at the end of the trusted window.
    TestScreens wrapper; wrapper.bind(&store,&time); TimeUs t2=0;
    openSettings(wrapper,t2); wrapper.handle(press(false),t2);
    for (int i=0;i<TimeService::MaxYear-2026;++i) wrapper.handle(press(false),t2);
    CHECK(wrapper.model().settings.fields[0]==TimeService::MaxYear);
    wrapper.handle(press(false),t2);
    CHECK(wrapper.model().settings.fields[0]==TimeService::MinYear);
}
// B held on an editor's field steps it again and again; the press belongs to
// the field it began on (docs/task12/plan.md 1.5, 2.3).
void holdInEditors() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    TimeUs now=0;
    auto holdB=[&](bool down) {
        Events e{}; e.holdChanged=true; e.hold=down ? Hold::B : Hold::None; e.holdSince=now;
        return e;
    };
    auto releaseB=[&](TimeUs since) {
        Events e{}; e.decide=true; e.pressUs=now-since; e.holdChanged=true; return e;
    };
    auto advance=[&](TimeUs to) { now=to; screens.update(now); };
    auto year=[&]() { return screens.model().settings.fields[0]; };
    openSettings(screens,now);
    screens.handle(press(false),now);              // the date editor, 2026
    // A short press steps once, on the release.
    TimeUs since=now;
    screens.handle(holdB(true),now);
    CHECK(screens.nextUpdate()==now+HoldRepeat::DelayUs && year()==2026);
    now+=200000; screens.handle(releaseB(since),now);
    CHECK(year()==2027 && screens.nextUpdate()==INT64_MAX);
    // Held: a step at 500ms, then one each 100ms, and the release adds none.
    since=now; screens.handle(holdB(true),now);
    advance(since+HoldRepeat::DelayUs-1);
    CHECK(year()==2027);
    advance(since+HoldRepeat::DelayUs);
    CHECK(year()==2028 && screens.nextUpdate()==since+HoldRepeat::DelayUs+HoldRepeat::PeriodUs);
    advance(since+HoldRepeat::DelayUs+HoldRepeat::PeriodUs);
    CHECK(year()==2029);
    // A stalled loop adds one step, not the ones it missed.
    advance(since+HoldRepeat::DelayUs+5*HoldRepeat::PeriodUs);
    CHECK(year()==2030 && screens.nextUpdate()==now+HoldRepeat::PeriodUs);
    screens.handle(releaseB(since),now);
    CHECK(year()==2030 && screens.nextUpdate()==INT64_MAX);
    // A joining makes a chord: the hold ends with no release, and nothing steps.
    since=now; screens.handle(holdB(true),now);
    screens.handle(holdB(false),now+100000);
    advance(since+HoldRepeat::DelayUs*2);
    CHECK(year()==2030 && screens.nextUpdate()==INT64_MAX);
    // A tap that moves the focus spends the press: no repeat, and its release
    // steps neither field.
    const auto frame=screens.model();
    const SettingsGeometry probe{{frame.viewport.width,frame.viewport.height},frame.settings.view,frame.settings.cursor};
    const Rect month=settingsFieldBox(probe,1);
    since=now; screens.handle(holdB(true),now);
    screens.handle(tap(month.x+month.w/2,month.y+month.h/2),now+100000);
    CHECK(screens.model().settings.cursor==1 && screens.nextUpdate()==INT64_MAX);
    advance(since+HoldRepeat::DelayUs*2);
    screens.handle(releaseB(since),now);
    CHECK(year()==2030 && screens.model().settings.fields[1]==9);
    // On a button nothing repeats, and the release carries it out as before:
    // a long press of cancel still cancels.
    while (screens.model().settings.cursor!=settingsSlotCount(SettingsView::DateTime)-1)
        screens.handle(press(true),now);
    since=now; screens.handle(holdB(true),now);
    CHECK(screens.nextUpdate()==INT64_MAX);
    now+=2000000; screens.handle(releaseB(since),now);
    CHECK(screens.model().settings.view==SettingsView::Menu);
    // A press held while a tap saves the editor does not open a menu row on
    // its release.
    screens.handle(press(true),now); screens.handle(press(false),now);
    CHECK(screens.model().settings.view==SettingsView::Brightness);
    const auto bright=screens.model();
    const SettingsGeometry brightProbe{{bright.viewport.width,bright.viewport.height},bright.settings.view,bright.settings.cursor};
    const Rect save=settingsButtonBox(brightProbe,0);
    since=now; screens.handle(holdB(true),now);
    screens.handle(tap(save.x+save.w/2,save.y+save.h/2),now+100000);
    CHECK(screens.model().settings.view==SettingsView::Menu);
    now+=300000; screens.handle(releaseB(since),now);
    CHECK(screens.model().settings.view==SettingsView::Menu && screens.model().screen==ScreenId::Settings);
    // The next press in the menu decides as usual.
    since=now; screens.handle(holdB(true),now);
    now+=100000; screens.handle(releaseB(since),now);
    CHECK(screens.model().settings.view==SettingsView::Brightness);
    // Leaving drops a hold: nothing keeps a deadline behind the clock.
    screens.handle(holdB(true),now);
    Events home{}; home.home=true; screens.handle(home,now);
    CHECK(screens.model().screen==ScreenId::Home && screens.nextUpdate()==INT64_MAX);
}
// The same through the real input: B held on a field, sampled every input
// period, steps it without the runtime polling the screen.
void heldInput() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    InputController input;
    TimeUs now=0;
    openSettings(screens,now);
    screens.handle(press(false),now);              // the date editor, 2026
    // As the runtime forwards it (HostRuntime::step).
    auto sample=[&](InputSnapshot raw) {
        const auto e=input.update(now,raw);
        if (e.home || e.next || e.decide || e.holdChanged || e.gesture!=Gesture::None) screens.handle(e,now);
        if (now>=screens.nextUpdate()) screens.update(now);
    };
    const TimeUs start=now;
    for (; now<=start+1000000; now+=10000) sample({false,true});
    sample({});
    // Steps at 500, 600, 700, 800, 900 and 1000ms, none on the release.
    CHECK(screens.model().settings.fields[0]==2032);
}
void saveAndCancel() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    TimeUs now=0;
    openSettings(screens,now);
    // Brightness: the model previews immediately and reverts on cancel.
    screens.handle(press(true),now); screens.handle(press(false),now);
    CHECK(screens.model().settings.view==SettingsView::Brightness);
    CHECK(screens.effectiveSettings().brightness==90);
    screens.handle(press(false),now);             // step the level
    CHECK(screens.model().settings.fields[0]==105 && screens.effectiveSettings().brightness==105);
    CHECK(screens.model().settings.savedBrightness==90); // The menu label still reflects storage.
    screens.handle(press(true),now);              // -> save
    screens.handle(press(true),now);              // -> cancel
    screens.handle(press(false),now);             // cancel
    CHECK(screens.model().settings.view==SettingsView::Menu);
    CHECK(screens.effectiveSettings().brightness==90 && store.get().brightness==90);
    // Same edit, confirmed this time.
    screens.handle(press(false),now);             // menu row 1 is still focused
    CHECK(screens.model().settings.view==SettingsView::Brightness);
    screens.handle(press(false),now);
    screens.handle(press(true),now);              // -> save
    screens.handle(press(false),now);
    CHECK(screens.model().settings.view==SettingsView::Menu);
    CHECK(store.get().brightness==105 && screens.effectiveSettings().brightness==105);
    CHECK(screens.model().settings.savedBrightness==105);
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,text::Saved)==0);
    // Home drops an unsaved preview as well.
    screens.handle(press(false),now);
    screens.handle(press(false),now);
    CHECK(screens.effectiveSettings().brightness==120);
    Events home{}; home.home=true;
    screens.handle(home,now);
    CHECK(screens.model().screen==ScreenId::Home && screens.effectiveSettings().brightness==105);
}
void dateSaving() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    TimeUs now=0;
    openSettings(screens,now);
    screens.handle(press(false),now); // date editor, 2026-09-21 00:00 JST
    // Tap the day's up arrow ten times: 21 -> 31, an impossible September date.
    const auto frame=screens.model();
    SettingsGeometry probe{{frame.viewport.width,frame.viewport.height},frame.settings.view,frame.settings.cursor};
    const Rect up=settingsArrowBox(probe,2,true);
    for (int i=0;i<10;++i) screens.handle(tap(up.x+up.w/2,up.y+up.h/2),now);
    CHECK(screens.model().settings.fields[2]==31);
    const Rect save=settingsButtonBox(probe,0);
    screens.handle(tap(save.x+save.w/2,save.y+save.h/2),now);
    CHECK(screens.model().settings.view==SettingsView::DateTime); // Refused.
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,text::InvalidDate)==0);
    // Back to a real date and save for real.
    const Rect down=settingsArrowBox(probe,2,false);
    for (int i=0;i<10;++i) screens.handle(tap(down.x+down.w/2,down.y+down.h/2),now);
    CHECK(screens.model().settings.fields[2]==21);
    screens.handle(tap(save.x+save.w/2,save.y+save.h/2),now);
    CHECK(screens.model().settings.view==SettingsView::Menu);
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,text::TimeSaved)==0);
    CHECK(hal.rtc.year==2026 && hal.rtc.month==9 && hal.rtc.day==20 && hal.rtc.hour==15);
    // An RTC that refuses the write reports it rather than showing success.
    openSettings(screens,now);
    screens.handle(press(false),now);
    hal.rtcWritable=false;
    screens.handle(tap(save.x+save.w/2,save.y+save.h/2),now);
    CHECK(screens.model().settings.view==SettingsView::DateTime);
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,text::SaveFailed)==0);
}
void runtimeApplies() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; StubRender render; TimeService time; time.begin(hal);
    DisplayDataSource data;
    HostApplication application(hal,render,data,468,468); auto& runtime=application.runtime();
    application.bindSettings(store,time);
    runtime.begin(); runtime.step();
    CHECK(hal.brightness==90 && hal.brightnessCalls==1);
    // Unchanged settings do not keep re-applying.
    for (int i=0;i<5;++i) { hal.time+=1000000; runtime.step(); }
    CHECK(hal.brightnessCalls==1);
    // Waking fades the level in, because the panel comes back dark: the first
    // frame is drawn at zero and the level eases out over 300ms.
    for (int i=0;i<31;++i) { hal.time+=1000000; runtime.step(); }
    CHECK(runtime.power().screenOff());
    hal.input={false,false,true,100,100}; hal.time+=10000; runtime.step();
    CHECK(!runtime.power().screenOff() && hal.brightnessCalls==2 && hal.brightness==0);
    hal.input={}; hal.time+=10000; runtime.step();     // release
    CHECK(hal.brightness>0 && hal.brightness<20);
    // Past the input's follow, the fade alone keeps the wait short.
    hal.time+=110000; runtime.step(); runtime.wait();
    CHECK(hal.brightness==58 && hal.waited<=16000);   // 120ms: 90*(1-0.6^2)
    // A level changed mid-fade becomes its end, without restarting it.
    CHECK(store.save({150,30}));
    runtime.dataChanged(); hal.time+=50000; runtime.step();
    CHECK(hal.brightness==122);                        // 170ms: 150*(1-(13/30)^2)
    hal.time+=130000; runtime.step();                  // 300ms: the fade ends
    CHECK(hal.brightness==150);
    // The fade leaves nothing behind: no more writes, no short wait.
    const int lit=hal.brightnessCalls;
    for (int i=0;i<5;++i) { hal.time+=16000; runtime.step(); }
    runtime.wait();
    CHECK(hal.brightnessCalls==lit && hal.waited>16000);
    // A shorter sleep timeout takes effect once it is stored.
    CHECK(store.save({90,15}));
    runtime.dataChanged(); runtime.step();             // a draw picks the value up
    hal.time+=16000000; runtime.step();
    CHECK(runtime.power().screenOff()); // 16s > 15s, which 30s would not have.
    // A step that runs past the fade's end still lands on the level: the fade
    // stays due until the full level is applied.
    hal.input={false,false,true,100,100}; hal.time+=10000; runtime.step();
    hal.input={}; hal.time+=10000; runtime.step();
    hal.time+=110000; runtime.step();
    CHECK(hal.brightness==58);
    hal.time+=181000; runtime.wait();                  // 301ms, with 90 not yet applied
    CHECK(hal.waited<=1000);
    runtime.step(); runtime.wait();
    CHECK(hal.brightness==90 && hal.waited>16000);
}
void fadeWaitsForPanel() {
    // A woken panel that shows only later keeps the level at zero until then,
    // and the whole fade follows from there.
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; hal.showsAfter=100000; StubRender render; TimeService time; time.begin(hal);
    DisplayDataSource data;
    HostApplication application(hal,render,data,468,468); auto& runtime=application.runtime();
    application.bindSettings(store,time);
    runtime.begin(); runtime.step();
    for (int i=0;i<31;++i) { hal.time+=1000000; runtime.step(); }
    CHECK(runtime.power().screenOff());
    hal.input={false,false,true,100,100}; hal.time+=10000; runtime.step();
    CHECK(!runtime.power().screenOff() && hal.brightness==0);
    hal.input={}; hal.time+=10000; runtime.step();     // release, 10ms
    CHECK(hal.brightness==0);
    hal.time+=80000; runtime.step(); runtime.wait();  // 90ms: dark, but stepped
    CHECK(hal.brightness==0 && hal.waited<=16000);
    hal.time+=130000; runtime.step();                  // 220ms: 120ms into the fade
    CHECK(hal.brightness==58);                         // 90*(1-0.6^2)
    hal.time+=180000; runtime.step();                  // 400ms: the fade ends
    CHECK(hal.brightness==90);
}
void statisticsAction() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    screens.setInfo("wararyoLauncher","0.1.0","5.5.0");
    TimeUs now=0;
    // Every other view keeps exactly the slots it had before actions existed.
    // The menu has none of its own: it is a list with its own selection.
    CHECK(settingsSlotCount(SettingsView::Menu)==0);
    CHECK(settingsSlotCount(SettingsView::DateTime)==7);
    CHECK(settingsSlotCount(SettingsView::Brightness)==3);
    CHECK(settingsSlotCount(SettingsView::ScreenOff)==3);
    CHECK(settingsActionCount(SettingsView::DateTime)==0);
    // Information gains one: the action, and then the single button.
    CHECK(settingsActionCount(SettingsView::Info)==1);
    CHECK(settingsSlotCount(SettingsView::Info)==2);
    openSettings(screens,now);
    CHECK(!screens.model().stats);
    for (int i=0;i<4;++i) screens.handle(press(true),now);  // menu row 4: 情報
    screens.handle(press(false),now);
    CHECK(screens.model().settings.view==SettingsView::Info);
    CHECK(screens.model().settings.cursor==0);
    // B takes the action and stays put: no notice, no view change.
    screens.handle(press(false),now);
    CHECK(screens.model().stats);
    CHECK(screens.model().settings.view==SettingsView::Info);
    CHECK(screens.model().toast==nullptr);
    // Pressing it again cannot undo it.
    screens.handle(press(false),now);
    CHECK(screens.model().stats && screens.model().settings.view==SettingsView::Info);
    // A moves on to the button, which still returns to the menu.
    screens.handle(press(true),now);
    CHECK(screens.model().settings.cursor==1);
    screens.handle(press(false),now);
    CHECK(screens.model().settings.view==SettingsView::Menu && screens.model().stats);
    // It outlives leaving the screen entirely.
    Events home{}; home.home=true; screens.handle(home,now);
    CHECK(screens.model().screen==ScreenId::Home && screens.model().stats);
    // A tap reaches the same action, and does not steal the button's box.
    TestScreens tapped; tapped.bind(&store,&time);
    tapped.setInfo("wararyoLauncher","0.1.0","5.5.0");
    TimeUs t2=0;
    openSettings(tapped,t2);
    for (int i=0;i<4;++i) tapped.handle(press(true),t2);
    tapped.handle(press(false),t2);
    CHECK(tapped.model().settings.view==SettingsView::Info && !tapped.model().stats);
    const auto tappedFrame=tapped.model();
    const SettingsGeometry infoGeometry{{tappedFrame.viewport.width,tappedFrame.viewport.height},
                                        tappedFrame.settings.view,tappedFrame.settings.cursor};
    const Rect action=settingsActionBox(infoGeometry,0);
    const Rect back=settingsButtonBox(infoGeometry,0);
    CHECK(!action.contains(back.x+back.w/2,back.y+back.h/2));
    tapped.handle(tap(action.x+action.w/2,action.y+action.h/2),t2);
    CHECK(tapped.model().stats && tapped.model().settings.view==SettingsView::Info);
    tapped.handle(tap(back.x+back.w/2,back.y+back.h/2),t2);
    CHECK(tapped.model().settings.view==SettingsView::Menu);
}
// The screen only asks for the statistics overlay: the setting is the
// application's, applied by the manager, and it outlives the screen.
void statisticsRequest() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    SettingsScreen screen; screen.resize(468,468); screen.bind(&store,&time);
    screen.enter(0);
    for (int i=0;i<4;++i) screen.handle(press(true),0);   // menu row 4: 情報
    screen.handle(press(false),0);
    CHECK(screen.model().view==SettingsView::Info);
    auto out=screen.handle(press(false),0);
    CHECK(out.enableStats && out.changed && !out.leave && out.notice==nullptr);
    // Asking again is harmless; the button only leaves and asks nothing.
    CHECK(screen.handle(press(false),0).enableStats);
    screen.handle(press(true),0);
    out=screen.handle(press(false),0);
    CHECK(!out.enableStats && screen.model().view==SettingsView::Menu);
    // Through the manager the request lands in the application's setting,
    // which the frame then reports, and it stays through home.
    TestScreens screens; screens.bind(&store,&time);
    TimeUs now=0;
    openSettings(screens,now);
    for (int i=0;i<4;++i) screens.handle(press(true),now);
    screens.handle(press(false),now);
    CHECK(!screens.runtime.stats);
    CHECK(screens.handle(press(false),now));
    CHECK(screens.runtime.stats && screens.model().stats);
    Events home{}; home.home=true; screens.handle(home,now);
    CHECK(screens.runtime.stats && screens.model().stats);
}
namespace {
bool near(float a,float b) { return std::abs(a-b)<0.001f; }
Events gesture(Gesture kind,int totalY=0,float velocityY=0) {
    Events e{}; e.gesture=kind; e.totalY=totalY; e.velocityY=velocityY; return e;
}
// Where the menu draws row `index` this frame: input and drawing share it.
RowLayout menuRow(const ScreenManager& s,int index) {
    const auto m=s.model();
    return layoutListRow(settingsMenuPlacement(m.viewport,m.settings.menu.scroll),index,false);
}
Events tapRow(const ScreenManager& s,int index) {
    const auto r=menuRow(s,index);
    return tap(r.box.x+r.box.w/2,r.centerY);
}
}
// The rows' ids, order and labels, built on the settings side.
void menuRows() {
    std::array<ListRow,SettingsMenuCount> rows{};
    SettingsModel model; model.savedBrightness=105; model.savedScreenOffSec=60;
    model.faceCount=2; model.faceNames[0]="Digital"; model.faceNames[1]="Forest"; model.currentFace=1;
    SettingsMenuLabels labels;
    const ListRows built=buildSettingsMenuRows(rows,&model,&labels);
    CHECK(built.count==6);
    // The watch face row names the face that shows (docs/task10/plan-10-5.md 5).
    const std::string expected[]={text::DateTime,menuLabel(text::Brightness,"105"),
                                 menuLabel(text::ScreenOff,seconds(60)),menuLabel(text::WatchFace,"Forest"),
                                 text::Info,text::Back};
    const SettingsView views[]={SettingsView::DateTime,SettingsView::Brightness,
                                SettingsView::ScreenOff,SettingsView::WatchFace,SettingsView::Info};
    for (int i=0;i<6;++i) {
        CHECK(built[i].label==expected[i]);
        CHECK(!built[i].icon && built[i].enabled && !built[i].dimmed);
        for (int j=0;j<i;++j) CHECK(built[j].id!=built[i].id);
        // Each row opens its view by id; only the last one leaves instead.
        SettingsView view=SettingsView::Menu;
        CHECK(settingsItemView(built[i].id,view)==(i<5));
        if (i<5) CHECK(view==views[i]);
    }
    // Without a face shown the row is just its name.
    model.currentFace=-1; buildSettingsMenuRows(rows,&model,&labels);
    CHECK(std::strcmp(rows[3].label,text::WatchFace)==0);
    // Input builds the same ids without labels.
    std::array<ListRow,SettingsMenuCount> ids{};
    buildSettingsMenuRows(ids);
    for (int i=0;i<6;++i) CHECK(ids[i].id==rows[i].id && ids[i].label[0]==0);
}
// The menu as the shared list: A, B and taps, scrolling, inertia, and the
// round trip through an editor (docs/task9/plan-9-3.md).
void menuList() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    screens.setInfo("wararyoLauncher","0.1.0","5.5.0");
    const float spacing=float(rowSpacing({468,468}));
    auto menu=[&] { return screens.model().settings.menu; };
    auto settle=[&](TimeUs& now) { now+=ListController::AnimationUs+20000; screens.update(now); };
    TimeUs now=0;
    openSettings(screens,now);
    // A visit starts on the first row, centred, with nothing moving.
    CHECK(menu().selection==0 && menu().scroll==0);
    CHECK(!screens.active() && screens.nextUpdate()==INT64_MAX);
    CHECK(screens.model().activity==FrameActivity::SettingsSingle);
    // A selects the next row and scrolls it to the middle, one frame at a time.
    screens.handle(press(true),now);
    CHECK(menu().selection==1 && menu().animating && screens.active());
    CHECK(screens.nextUpdate()==now+ListController::FrameUs);
    CHECK(screens.model().activity==FrameActivity::SettingsScroll);
    now+=ListController::FrameUs; CHECK(screens.update(now));
    CHECK(menu().scroll>0 && menu().scroll<spacing);
    settle(now);
    CHECK(near(menu().scroll,spacing) && !screens.active() && screens.nextUpdate()==INT64_MAX);
    // A late frame lands where the time says, not on the next step in line.
    screens.handle(press(true),now);
    now+=ListController::AnimationUs*3; screens.update(now);
    CHECK(near(menu().scroll,2*spacing) && !menu().animating && screens.nextUpdate()==INT64_MAX);
    // From the last row A wraps to the first.
    screens.handle(press(true),now); settle(now);
    screens.handle(press(true),now); settle(now);
    screens.handle(press(true),now); settle(now);
    CHECK(menu().selection==5 && near(menu().scroll,5*spacing));
    screens.handle(press(true),now); settle(now);
    CHECK(menu().selection==0 && near(menu().scroll,0));
    // At the top a pull down only springs back: settings has no edge gesture.
    screens.handle(gesture(Gesture::DragStart,40),now);
    screens.handle(gesture(Gesture::DragMove,300),now+10000);
    screens.handle(gesture(Gesture::DragEnd,300,3000),now+20000);
    now+=20000; settle(now);
    CHECK(screens.model().screen==ScreenId::Settings && near(menu().scroll,0) && menu().selection==0);
    // A drag scrolls without deciding anything, then settles on a row.
    screens.handle(gesture(Gesture::DragStart,-20),now);
    screens.handle(gesture(Gesture::DragMove,-int(2*spacing)),now+10000);
    CHECK(menu().dragging && screens.active() && near(menu().scroll,2*spacing));
    CHECK(screens.model().activity==FrameActivity::SettingsScroll);
    screens.handle(gesture(Gesture::DragEnd,-int(2*spacing),0),now+20000);
    now+=20000; settle(now);
    CHECK(screens.model().settings.view==SettingsView::Menu && !screens.model().toast);
    CHECK(menu().selection==2 && near(menu().scroll,2*spacing) && !screens.active());
    // Horizontal drags are not the list's.
    Events sideways=gesture(Gesture::DragStart,5); sideways.totalX=80;
    CHECK(!screens.handle(sideways,now) && !menu().dragging);
    screens.handle(gesture(Gesture::DragEnd,5),now);
    // A tap hits the row where it is drawn while the list is moving: A starts
    // a scroll, and a tap part way hits row 5 at its current place.
    screens.handle(press(true),now);
    now+=3*ListController::FrameUs; screens.update(now);
    CHECK(menu().animating && menu().scroll>2*spacing && menu().scroll<3*spacing);
    {
        const auto target=menuRow(screens,5),neighbour=menuRow(screens,4);
        CHECK(!target.box.empty() && !neighbour.box.empty());
        CHECK(!target.box.contains(neighbour.box.x+neighbour.box.w/2,neighbour.centerY));
    }
    screens.handle(gesture(Gesture::TouchStart),now);
    CHECK(menu().animating); // A retarget, not inertia: the touch does not stop it.
    screens.handle(tapRow(screens,5),now);
    CHECK(screens.model().screen==ScreenId::AppList); // Row 5 is 戻る.
    // A new visit is back at the top, however the last one ended.
    openSettings(screens,now);
    CHECK(menu().selection==0 && menu().scroll==0 && !screens.active());
    // Inertia: a touch stops it, and neither that tap nor that drag decides.
    screens.handle(gesture(Gesture::DragStart,-20),now);
    screens.handle(gesture(Gesture::DragMove,-60),now+10000);
    screens.handle(gesture(Gesture::DragEnd,-60,-1500),now+20000);
    now+=20000+2*ListController::FrameUs; screens.update(now);
    CHECK(menu().animating && screens.active());
    CHECK(screens.handle(gesture(Gesture::TouchStart),now));
    const float held=menu().scroll;
    CHECK(!screens.active() && screens.nextUpdate()==INT64_MAX);
    now+=100000; screens.update(now); CHECK(menu().scroll==held);
    const int under=int(std::lround(held/spacing));
    screens.handle(tapRow(screens,under),now);
    CHECK(screens.model().settings.view==SettingsView::Menu && !screens.model().toast);
    settle(now);
    CHECK(!screens.active() && near(menu().scroll,menu().selection*spacing));
    // The same stop, then a drag: it scrolls again and still decides nothing.
    screens.handle(gesture(Gesture::DragStart,-20),now);
    screens.handle(gesture(Gesture::DragEnd,-20,-1500),now+10000);
    now+=10000+ListController::FrameUs; screens.update(now);
    screens.handle(gesture(Gesture::TouchStart),now);
    screens.handle(gesture(Gesture::DragStart,15),now+10000);
    screens.handle(gesture(Gesture::DragEnd,15,0),now+20000);
    now+=20000; settle(now);
    CHECK(screens.model().settings.view==SettingsView::Menu && !screens.active());
    // Tap 消灯時間 while the list settles on another row. The editor opens with
    // the menu at rest: no deadline and no activity behind it.
    screens.handle(gesture(Gesture::DragStart,-20),now);
    screens.handle(gesture(Gesture::DragMove,-int(spacing)-30),now+10000);
    screens.handle(gesture(Gesture::DragEnd,-int(spacing)-30,0),now+20000);
    now+=20000+ListController::FrameUs; screens.update(now);
    CHECK(menu().animating);
    screens.handle(tapRow(screens,2),now);
    auto m=screens.model();
    CHECK(m.settings.view==SettingsView::ScreenOff && m.settings.menu.selection==2);
    CHECK(!m.settings.menu.animating && !screens.active() && screens.nextUpdate()==INT64_MAX);
    CHECK(m.activity==FrameActivity::SettingsSingle);
    const float kept=m.settings.menu.scroll;
    // In an editor the list is out of play: A and B belong to the editor.
    screens.handle(press(true),now);
    CHECK(screens.model().settings.cursor==1 && screens.model().settings.menu.selection==2);
    // Cancel returns to the same row at the same scroll.
    screens.handle(press(true),now); screens.handle(press(false),now);
    m=screens.model();
    CHECK(m.settings.view==SettingsView::Menu && m.settings.menu.selection==2);
    CHECK(m.settings.menu.scroll==kept && !screens.active());
    // So does information, from a row picked by A mid-scroll.
    screens.handle(press(true),now); screens.handle(press(true),now);
    now+=2*ListController::FrameUs; screens.update(now);
    screens.handle(press(false),now);
    m=screens.model();
    CHECK(m.settings.view==SettingsView::Info && m.settings.menu.selection==4);
    CHECK(near(m.settings.menu.scroll,4*spacing) && screens.nextUpdate()==INT64_MAX);
    screens.handle(press(true),now); screens.handle(press(false),now); // -> 戻る
    m=screens.model();
    CHECK(m.settings.view==SettingsView::Menu && m.settings.menu.selection==4);
    CHECK(near(m.settings.menu.scroll,4*spacing));
    // Home in the middle of a scroll leaves nothing of settings running.
    screens.handle(press(true),now);
    CHECK(screens.active());
    Events home{}; home.home=true; screens.handle(home,now);
    CHECK(screens.model().screen==ScreenId::Home && !screens.active() && screens.nextUpdate()==INT64_MAX);
    now+=1000000; CHECK(!screens.update(now));
    // So does 戻る, decided while A is still scrolling to it; the launcher
    // is back on its settings row, where it was left.
    openSettings(screens,now);
    const int launcherRow=screens.model().launcher.list.selection;
    const float launcherScroll=screens.model().launcher.list.scroll;
    for (int i=0;i<5;++i) screens.handle(press(true),now);
    CHECK(screens.active());
    screens.handle(press(false),now);
    m=screens.model();
    CHECK(m.screen==ScreenId::AppList && !screens.active() && screens.nextUpdate()==INT64_MAX);
    CHECK(m.launcher.list.selection==launcherRow && m.launcher.list.scroll==launcherScroll);
    CHECK(m.activity==FrameActivity::Single);
    // Only the launcher's pull down from the top of its own list goes home.
    Events a{}; a.next=true;
    while (screens.model().launcher.list.selection!=0) { screens.handle(a,now); settle(now); }
    screens.handle(gesture(Gesture::DragStart,40),now);
    screens.handle(gesture(Gesture::DragEnd,120,0),now+10000);
    CHECK(screens.model().screen==ScreenId::Home);
}
// Save and failure on the menu labels, and the preview they must not show.
void menuLabels() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    TestScreens screens; screens.bind(&store,&time);
    TimeUs now=0;
    auto label=[&](int row) {
        std::array<ListRow,SettingsMenuCount> rows{};
        SettingsMenuLabels labels;
        const auto model=screens.model().settings;
        buildSettingsMenuRows(rows,&model,&labels);
        return std::string(rows[row].label);
    };
    openSettings(screens,now);
    CHECK(label(1)==menuLabel(text::Brightness,"90") && label(2)==menuLabel(text::ScreenOff,seconds(30)));
    // A failed save keeps the editor open with the edit, and the label.
    screens.handle(press(true),now); screens.handle(press(false),now);
    screens.handle(press(false),now);
    CHECK(screens.model().settings.fields[0]==105 && label(1)==menuLabel(text::Brightness,"90"));
    backend.writable=false;
    screens.handle(press(true),now); screens.handle(press(false),now); // save
    auto m=screens.model();
    CHECK(m.settings.view==SettingsView::Brightness && m.settings.fields[0]==105);
    CHECK(m.toast && std::strcmp(m.toast,text::SaveFailed)==0);
    CHECK(screens.effectiveSettings().brightness==105 && label(1)==menuLabel(text::Brightness,"90"));
    // The retry succeeds and the label follows, on the row it left from.
    backend.writable=true;
    screens.handle(press(false),now);
    m=screens.model();
    CHECK(m.settings.view==SettingsView::Menu && m.settings.menu.selection==1 && label(1)==menuLabel(text::Brightness,"105"));
    // Cancel restores the preview and leaves the label alone.
    screens.handle(press(true),now); now+=200000; screens.update(now);
    screens.handle(press(false),now);
    CHECK(screens.model().settings.view==SettingsView::ScreenOff);
    screens.handle(press(false),now);
    screens.handle(press(true),now); screens.handle(press(true),now); screens.handle(press(false),now);
    m=screens.model();
    CHECK(m.settings.view==SettingsView::Menu && m.settings.menu.selection==2 && label(2)==menuLabel(text::ScreenOff,seconds(30)));
    CHECK(screens.effectiveSettings().screenOffSec==30);
}
// The runtime draws the menu's scroll from its deadline, stops when it ends,
// does not draw with the panel off and lands the scroll on waking.
void runtimeMenuScroll() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; StubRender render; TimeService time; time.begin(hal);
    DisplayDataSource data;
    HostApplication application(hal,render,data,468,468); auto& runtime=application.runtime();
    application.bindSettings(store,time);
    runtime.begin(); runtime.step();
    const float spacing=float(rowSpacing({468,468}));
    // A press and its release.
    auto click=[&](bool a) {
        hal.input={}; hal.input.a=a; hal.input.b=!a; runtime.step();
        hal.time+=20000; hal.input={}; runtime.step();
    };
    auto idle=[&](TimeUs span) {
        const TimeUs end=hal.time+span;
        while (hal.time<end) { hal.time+=4000; runtime.step(); }
    };
    // The wait right after a USB poll, which is then a full period away: any
    // shorter wait is a deadline somebody left behind.
    auto quietWait=[&] {
        const int polls=hal.usbPolls;
        while (hal.usbPolls==polls) { hal.time+=4000; runtime.step(); }
        runtime.wait(); return hal.waited;
    };
    click(true); idle(300000);                       // clock -> list, stopwatch row
    click(true); idle(300000);                       // timer row
    click(true); idle(300000);                       // settings row
    click(false); idle(300000);
    CHECK(runtime.model().screen==ScreenId::Settings);
    CHECK(quietWait()==1000000);
    // A scrolls the menu: frames keep coming from its deadline alone, and the
    // display counts as active until it lands.
    const int before=render.draws;
    click(true);
    hal.time+=ListController::FrameUs; runtime.step();
    CHECK(runtime.power().state()==DisplayState::Active);
    idle(ListController::AnimationUs);
    CHECK(render.draws-before>=8);
    CHECK(near(runtime.model().settings.menu.scroll,spacing) && !runtime.model().settings.menu.animating);
    idle(200000);
    CHECK(runtime.power().state()==DisplayState::WatchIdle);
    const int settled=render.draws;
    idle(500000);
    CHECK(render.draws==settled);
    CHECK(quietWait()==1000000);
    // The panel goes dark in the middle of a scroll (a long overrun): no
    // frame is drawn for it, and its deadline does not shorten the wait.
    click(true);
    hal.time+=31000000; runtime.step();
    CHECK(runtime.power().screenOff());
    const int dark=render.draws;
    for (int i=0;i<20;++i) {
        hal.time+=16000; runtime.step(); runtime.wait();
        CHECK(hal.waited>ListController::FrameUs);
    }
    CHECK(render.draws==dark);
    // Waking lands the scroll at once instead of replaying it, and leaves no
    // frame deadline behind.
    hal.input={}; hal.input.touching=true; hal.input.x=hal.input.y=234; runtime.step();
    CHECK(!runtime.power().screenOff() && render.draws==dark+1);
    const auto woke=runtime.model().settings.menu;
    CHECK(woke.selection==2 && near(woke.scroll,2*spacing) && !woke.animating);
    hal.time+=20000; hal.input={}; runtime.step();
    CHECK(runtime.model().screen==ScreenId::Settings && runtime.model().settings.view==SettingsView::Menu);
    idle(300000);
    CHECK(quietWait()==1000000);
}
int main() {
    storeRecord(); menuAndEditors(); holdInEditors(); heldInput(); saveAndCancel(); dateSaving(); runtimeApplies(); fadeWaitsForPanel();
    statisticsAction(); statisticsRequest(); menuRows(); menuList(); menuLabels(); runtimeMenuScroll();
    std::cout << "PASS: settings record, menu/editors, held B, save/cancel, date saving, "
                 "runtime apply, fade after panel shows, statistics action/request, menu rows/list/labels, "
                 "runtime menu scroll\n";
}
