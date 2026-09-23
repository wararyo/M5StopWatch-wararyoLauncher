#include "app/AppRuntime.h"
#include "app/AppRegistry.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
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
    TimeUs time=0;
    InputSnapshot input{};
    TimeUs now() override { return time; }
    InputSnapshot sampleInput() override { return input; }
    UsbState sampleUsb() override { return {}; }
    void setScreenOff(bool) override {}
    void waitUs(TimeUs) override {}
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
    void draw(const ScreenModel&,const WatchData&) override { ++draws; }
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
    while (AppRegistry[s.model().list.selection].id!=AppId::Settings) { s.handle(e,now); now+=200000; s.update(now); }
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
    ScreenManager screens; screens.bind(&store,&time);
    screens.setInfo("wararyoLauncher","0.1.0","5.5.0");
    TimeUs now=0;
    openSettings(screens,now);
    CHECK(screens.model().settings.view==SettingsView::Menu);
    // A cycles the five rows and wraps.
    for (int i=0;i<4;++i) screens.handle(press(true),now);
    CHECK(screens.model().settings.cursor==4);
    screens.handle(press(true),now);
    CHECK(screens.model().settings.cursor==0);
    // The last row leaves back to the list, keeping its selection.
    const int listRow=screens.model().list.selection;
    for (int i=0;i<4;++i) screens.handle(press(true),now);
    screens.handle(press(false),now);
    CHECK(screens.model().screen==ScreenId::AppList && screens.model().list.selection==listRow);
    openSettings(screens,now);
    // Row 0 opens the date editor, seeded with the current time.
    screens.handle(press(false),now);
    const auto& s=screens.model().settings;
    CHECK(s.view==SettingsView::DateTime && s.cursor==0 && !s.editing);
    CHECK(s.fields[0]==2026 && s.fields[1]==9 && s.fields[2]==21 && s.fields[3]==0 && s.fields[4]==0);
    // B enters the field, A steps the value, B confirms the field.
    screens.handle(press(false),now);
    CHECK(screens.model().settings.editing);
    screens.handle(press(true),now);
    CHECK(screens.model().settings.fields[0]==2027 && screens.model().settings.cursor==0);
    screens.handle(press(false),now);
    CHECK(!screens.model().settings.editing);
    screens.handle(press(true),now);
    CHECK(screens.model().settings.cursor==1); // Not editing: A moves on.
    // The year wraps at the end of the trusted window.
    ScreenManager wrapper; wrapper.bind(&store,&time); TimeUs t2=0;
    openSettings(wrapper,t2); wrapper.handle(press(false),t2);
    wrapper.handle(press(false),t2); // edit the year
    for (int i=0;i<TimeService::MaxYear-2026;++i) wrapper.handle(press(true),t2);
    CHECK(wrapper.model().settings.fields[0]==TimeService::MaxYear);
    wrapper.handle(press(true),t2);
    CHECK(wrapper.model().settings.fields[0]==TimeService::MinYear);
}
void saveAndCancel() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    ScreenManager screens; screens.bind(&store,&time);
    TimeUs now=0;
    openSettings(screens,now);
    // Brightness: the model previews immediately and reverts on cancel.
    screens.handle(press(true),now); screens.handle(press(false),now);
    CHECK(screens.model().settings.view==SettingsView::Brightness);
    CHECK(screens.effectiveSettings().brightness==90);
    screens.handle(press(false),now); screens.handle(press(true),now);
    CHECK(screens.model().settings.fields[0]==105 && screens.effectiveSettings().brightness==105);
    CHECK(screens.model().settings.savedBrightness==90); // The menu label still reflects storage.
    screens.handle(press(false),now);             // leave the field
    screens.handle(press(true),now);              // -> save
    screens.handle(press(true),now);              // -> cancel
    screens.handle(press(false),now);             // cancel
    CHECK(screens.model().settings.view==SettingsView::Menu);
    CHECK(screens.effectiveSettings().brightness==90 && store.get().brightness==90);
    // Same edit, confirmed this time.
    screens.handle(press(false),now);             // menu row 1 is still focused
    CHECK(screens.model().settings.view==SettingsView::Brightness);
    screens.handle(press(false),now); screens.handle(press(true),now);
    screens.handle(press(false),now);
    screens.handle(press(true),now);              // -> save
    screens.handle(press(false),now);
    CHECK(screens.model().settings.view==SettingsView::Menu);
    CHECK(store.get().brightness==105 && screens.effectiveSettings().brightness==105);
    CHECK(screens.model().settings.savedBrightness==105);
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,"保存しました")==0);
    // Home drops an unsaved preview as well.
    screens.handle(press(false),now);
    screens.handle(press(false),now); screens.handle(press(true),now);
    CHECK(screens.effectiveSettings().brightness==120);
    Events home{}; home.home=true;
    screens.handle(home,now);
    CHECK(screens.model().screen==ScreenId::Home && screens.effectiveSettings().brightness==105);
}
void dateSaving() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    ScreenManager screens; screens.bind(&store,&time);
    TimeUs now=0;
    openSettings(screens,now);
    screens.handle(press(false),now); // date editor, 2026-09-21 00:00 JST
    // Tap the day's up arrow ten times: 21 -> 31, an impossible September date.
    const auto frame=screens.model();
    SettingsGeometry probe{{frame.width,frame.height},frame.settings.view,frame.settings.cursor};
    const Rect up=settingsArrowBox(probe,2,true);
    for (int i=0;i<10;++i) screens.handle(tap(up.x+up.w/2,up.y+up.h/2),now);
    CHECK(screens.model().settings.fields[2]==31);
    const Rect save=settingsButtonBox(probe,0);
    screens.handle(tap(save.x+save.w/2,save.y+save.h/2),now);
    CHECK(screens.model().settings.view==SettingsView::DateTime); // Refused.
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,"日付が正しくありません")==0);
    // Back to a real date and save for real.
    const Rect down=settingsArrowBox(probe,2,false);
    for (int i=0;i<10;++i) screens.handle(tap(down.x+down.w/2,down.y+down.h/2),now);
    CHECK(screens.model().settings.fields[2]==21);
    screens.handle(tap(save.x+save.w/2,save.y+save.h/2),now);
    CHECK(screens.model().settings.view==SettingsView::Menu);
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,"時刻を保存しました")==0);
    CHECK(hal.rtc.year==2026 && hal.rtc.month==9 && hal.rtc.day==20 && hal.rtc.hour==15);
    // An RTC that refuses the write reports it rather than showing success.
    openSettings(screens,now);
    screens.handle(press(false),now);
    hal.rtcWritable=false;
    screens.handle(tap(save.x+save.w/2,save.y+save.h/2),now);
    CHECK(screens.model().settings.view==SettingsView::DateTime);
    CHECK(screens.model().toast && std::strcmp(screens.model().toast,"保存に失敗しました")==0);
}
void runtimeApplies() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; StubRender render; TimeService time; time.begin(hal);
    DisplayDataSource data;
    AppRuntime runtime(hal,render,data,468,468);
    runtime.bindSettings(store,time);
    runtime.begin(); runtime.step();
    CHECK(hal.brightness==90 && hal.brightnessCalls==1);
    // Unchanged settings do not keep re-applying.
    for (int i=0;i<5;++i) { hal.time+=1000000; runtime.step(); }
    CHECK(hal.brightnessCalls==1);
    // Waking re-applies, because the panel comes back dark.
    for (int i=0;i<31;++i) { hal.time+=1000000; runtime.step(); }
    CHECK(runtime.power().screenOff());
    hal.input={false,false,true,100,100}; hal.time+=10000; runtime.step();
    CHECK(!runtime.power().screenOff() && hal.brightnessCalls==2 && hal.brightness==90);
    // A shorter sleep timeout takes effect once it is stored.
    CHECK(store.save({90,15}));
    hal.input={}; hal.time+=10000; runtime.step();     // release
    runtime.dataChanged(); runtime.step();             // a draw picks the value up
    hal.time+=16000000; runtime.step();
    CHECK(runtime.power().screenOff()); // 16s > 15s, which 30s would not have.
}
void statisticsAction() {
    MemoryBackend backend; SettingsStore store; store.begin(backend);
    StubHal hal; TimeService time; time.begin(hal);
    ScreenManager screens; screens.bind(&store,&time);
    screens.setInfo("wararyoLauncher","0.1.0","5.5.0");
    TimeUs now=0;
    // Every other view keeps exactly the slots it had before actions existed.
    CHECK(settingsSlotCount(SettingsView::Menu)==SettingsMenuRows);
    CHECK(settingsSlotCount(SettingsView::DateTime)==7);
    CHECK(settingsSlotCount(SettingsView::Brightness)==3);
    CHECK(settingsSlotCount(SettingsView::ScreenOff)==3);
    CHECK(settingsActionCount(SettingsView::DateTime)==0);
    // Information gains one: the action, and then the single button.
    CHECK(settingsActionCount(SettingsView::Info)==1);
    CHECK(settingsSlotCount(SettingsView::Info)==2);
    openSettings(screens,now);
    CHECK(!screens.model().stats);
    for (int i=0;i<3;++i) screens.handle(press(true),now);  // menu row 3: 情報
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
    ScreenManager tapped; tapped.bind(&store,&time);
    tapped.setInfo("wararyoLauncher","0.1.0","5.5.0");
    TimeUs t2=0;
    openSettings(tapped,t2);
    for (int i=0;i<3;++i) tapped.handle(press(true),t2);
    tapped.handle(press(false),t2);
    CHECK(tapped.model().settings.view==SettingsView::Info && !tapped.model().stats);
    const auto tappedFrame=tapped.model();
    const SettingsGeometry infoGeometry{{tappedFrame.width,tappedFrame.height},
                                        tappedFrame.settings.view,tappedFrame.settings.cursor};
    const Rect action=settingsActionBox(infoGeometry,0);
    const Rect back=settingsButtonBox(infoGeometry,0);
    CHECK(!action.contains(back.x+back.w/2,back.y+back.h/2));
    tapped.handle(tap(action.x+action.w/2,action.y+action.h/2),t2);
    CHECK(tapped.model().stats && tapped.model().settings.view==SettingsView::Info);
    tapped.handle(tap(back.x+back.w/2,back.y+back.h/2),t2);
    CHECK(tapped.model().settings.view==SettingsView::Menu);
}
int main() {
    storeRecord(); menuAndEditors(); saveAndCancel(); dateSaving(); runtimeApplies();
    statisticsAction();
    std::cout << "PASS: settings record, menu/editors, save/cancel, date saving, "
                 "runtime apply, statistics action\n";
}
