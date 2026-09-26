#include "host/HostApplication.h"
#include "features/home/HomeDataSource.h"
#include "features/background/BackgroundInfoHub.h"
#include "features/stopwatch/StopwatchBackgroundInfo.h"
#include "services/TimeService.h"
#include "assets/AppIcons.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
constexpr TimeUs Second=1000000,Minute=60*Second,Hour=60*Minute;
struct FakeProvider : BackgroundInfoProvider {
    LaunchTargetId appId;
    std::string text;
    TimeUs due=INT64_MAX;
    bool present=true;
    // Written verbatim when set, to hand the hub an unterminated buffer.
    const char* raw=nullptr;
    LaunchTargetId writtenId{};
    bool lies=false;
    const IconBitmap* icon=nullptr;
    std::optional<uint16_t> color{};
    mutable int samples=0;
    FakeProvider(LaunchTargetId id,std::string label):appId(id),text(std::move(label)) {}
    LaunchTargetId id() const override { return appId; }
    bool sample(TimeUs,BackgroundInfo& out) const override {
        ++samples;
        if (!present) return false;
        if (raw) std::memcpy(out.label,raw,sizeof(out.label));
        else std::snprintf(out.label,sizeof(out.label),"%s",text.c_str());
        if (lies) out.appId=writtenId;
        out.icon=icon; out.suggestedColor=color;
        out.nextChangeAt=due;
        return true;
    }
};
std::string label(const BackgroundSnapshot& s,int i) { return s.items[i].label; }
std::string format(TimeUs elapsed,TimeUs* wait=nullptr) {
    char text[BackgroundLabelBytes];
    const auto w=formatStopwatchBackground(elapsed,text,sizeof(text));
    if (wait) *wait=w;
    return text;
}
struct StubHal : Hal {
    CivilTime rtc{2026,9,20,15,0,0};
    int64_t clockUs=0;
    TimeUs time=0;
    InputSnapshot input{};
    int sleeps=0;
    TimeUs now() override { return time; }
    InputSnapshot sampleInput() override { return input; }
    UsbState sampleUsb() override { return {}; }
    void setScreenOff(bool off) override { if (off) ++sleeps; }
    void waitUs(TimeUs) override {}
    bool inputPending() override { return input.a || input.b || input.touching; }
    bool readRtc(CivilTime& utc) override { utc=rtc; return true; }
    bool writeRtc(const CivilTime& utc) override { rtc=utc; return true; }
    void setUtcClock(int64_t seconds) override { clockUs=seconds*1000000; }
    int64_t utcClockUs() override { return clockUs; }
    BatteryState sampleBattery() override { return {}; }
    void setBrightness(int) override {}
};
// Keeps what the last frame was given, as the watch face would.
struct RecordingRender : RenderPort {
    int draws=0;
    FrameModel model{};
    WatchData watch{};
    void invalidate() override {}
    void draw(const FrameModel& m,const WatchData& d) override { ++draws; model=m; watch=d; }
    TimeUs nextUpdate(TimeUs now,const WatchData& d) const override { return nextMinute(now,d); }
    // A face that shows the first two items, as Digital will (task 10-3).
    bool showLabels=false;
    BackgroundInterest backgroundInterest(const WatchData& d) const override {
        return showLabels ? leadingItems(d.background,2) : BackgroundInterest{};
    }
};
}

void collectsInRegistrationOrder() {
    BackgroundInfoHub hub;
    CHECK(hub.collect(0)==BackgroundUnchanged && hub.snapshot().count==0);   // no providers
    FakeProvider a(LaunchTargetId::External2,"a"),b(LaunchTargetId::Stopwatch,"b");
    FakeProvider c(LaunchTargetId::Settings,"c"),d(LaunchTargetId::External1,"d");
    // An id the launcher does not know is carried as it is; a face may fall
    // back to a generic look for it.
    FakeProvider unknown(static_cast<LaunchTargetId>(42),"?");
    CHECK(hub.add(a)==BackgroundInfoHub::AddResult::Added);
    CHECK(hub.collect(0)==BackgroundAdded);
    CHECK(hub.snapshot().count==1 && label(hub.snapshot(),0)=="a");
    CHECK(hub.snapshot().items[0].appId==LaunchTargetId::External2);
    // Registration order, not id order, is the display order.
    CHECK(hub.add(b)==BackgroundInfoHub::AddResult::Added);
    CHECK(hub.add(c)==BackgroundInfoHub::AddResult::Added);
    CHECK(hub.add(unknown)==BackgroundInfoHub::AddResult::Added);
    hub.collect(0);
    const auto& s=hub.snapshot();
    CHECK(s.count==4 && label(s,0)=="a" && label(s,1)=="b" && label(s,2)=="c" && label(s,3)=="?");
    CHECK(s.items[3].appId==static_cast<LaunchTargetId>(42));
    // Past the capacity, and a second provider for an id, are refused and counted.
    CHECK(hub.add(d)==BackgroundInfoHub::AddResult::Full);
    FakeProvider again(LaunchTargetId::Stopwatch,"again");
    CHECK(hub.add(again)==BackgroundInfoHub::AddResult::Duplicate);
    CHECK(hub.providers()==4 && hub.rejected()==2);
    hub.collect(0);
    CHECK(hub.snapshot().count==4 && d.samples==0 && again.samples==0);
    // A gap keeps the others in order.
    b.present=false;
    CHECK(hub.collect(0)==BackgroundRemoved);
    CHECK(hub.snapshot().count==3 && label(hub.snapshot(),0)=="a" && label(hub.snapshot(),1)=="c");
    CHECK(hub.collect(0)==BackgroundUnchanged);
    b.present=true;
    CHECK(hub.collect(0)==BackgroundAdded && label(hub.snapshot(),1)=="b");
    // The id is the one the provider registered under, whatever it writes.
    BackgroundInfoHub other; FakeProvider liar(LaunchTargetId::External3,"x");
    liar.lies=true; liar.writtenId=LaunchTargetId::Settings;
    other.add(liar); other.collect(0);
    CHECK(other.snapshot().items[0].appId==LaunchTargetId::External3);
}

void labelsAreCheckedAndOwned() {
    BackgroundInfoHub hub;
    FakeProvider p(LaunchTargetId::Stopwatch,"");
    hub.add(p);
    // An empty label is no information, however it was returned.
    CHECK(hub.collect(0)==BackgroundUnchanged && hub.snapshot().count==0);
    // The longest label that fits is kept whole.
    p.text=std::string(BackgroundLabelBytes-1,'x');
    hub.collect(0);
    CHECK(label(hub.snapshot(),0)==p.text);
    // An unterminated buffer is cut to fit.
    char full[BackgroundLabelBytes]; std::memset(full,'y',sizeof(full));
    p.raw=full; hub.collect(0);
    CHECK(label(hub.snapshot(),0)==std::string(BackgroundLabelBytes-1,'y'));
    // ...and never through a character: 15 three-byte characters are 45
    // bytes, and a 16th would end at byte 48, past the 47 that fit.
    char kana[BackgroundLabelBytes];
    for (int i=0;i<16;++i) std::memcpy(kana+3*i,"\xe3\x81\x82",3);
    p.raw=kana; hub.collect(0);
    CHECK(std::strlen(hub.snapshot().items[0].label)==45);
    CHECK(label(hub.snapshot(),0)==std::string(kana,45));
    // A provider's own snprintf cut inside a character is repaired the same way.
    p.raw=nullptr; p.text="12345"+std::string(14,'-')+std::string(kana,30);  // 19+30 bytes
    hub.collect(0);
    CHECK(std::strlen(hub.snapshot().items[0].label)==46);
    // A label of nothing but a broken character is empty, so no item.
    char broken[BackgroundLabelBytes]{}; broken[0]='\xe3'; broken[1]='\x81';
    p.raw=broken; hub.collect(0);
    CHECK(hub.snapshot().count==0);
    // The frame keeps its copy while the provider moves on.
    p.raw=nullptr; p.text="12:34"; p.due=5*Second;
    CHECK(hub.collect(0)==BackgroundAdded);
    WatchData frame{}; frame.background=hub.snapshot();
    p.text="12:35";
    CHECK(label(frame.background,0)=="12:34" && label(hub.snapshot(),0)=="12:34");
    CHECK(hub.collect(Second)==BackgroundRelabeled);
    CHECK(label(frame.background,0)=="12:34" && label(hub.snapshot(),0)=="12:35");
    // A deadline that moved on while the text stayed is not a change.
    p.due=9*Second;
    CHECK(hub.collect(2*Second)==BackgroundUnchanged && hub.snapshot().items[0].nextChangeAt==9*Second);
    // Several kinds at once.
    BackgroundInfoHub two; FakeProvider x(LaunchTargetId::Settings,"x"),y(LaunchTargetId::External1,"y");
    two.add(x); two.add(y); y.present=false; two.collect(0);
    x.present=false; y.present=true;
    CHECK(two.collect(0)==(BackgroundAdded|BackgroundRemoved));
    // The notification is held until the next collection.
    CHECK(!two.pending()); two.invalidate(); CHECK(two.pending());
    two.collect(0); CHECK(!two.pending());
}

void iconsAndColoursComeFromTheApp() {
    static const uint8_t mask[6]={0,64,128,192,255,255};
    static const IconBitmap timerIcon{mask,3,2},otherIcon{mask,2,3};
    BackgroundInfoHub hub;
    FakeProvider p(LaunchTargetId::External1,"12:34");
    hub.add(p);
    // No icon and no colour are carried as such: the face falls back to its own.
    CHECK(hub.collect(0)==BackgroundAdded);
    CHECK(hub.snapshot().items[0].icon==nullptr && !hub.snapshot().items[0].suggestedColor);
    // Only the reference is copied, and a restyle is a change of its own kind.
    p.icon=&timerIcon; p.color=0x349f;
    CHECK(hub.collect(0)==BackgroundRestyled);
    CHECK(hub.snapshot().items[0].icon==&timerIcon && hub.snapshot().items[0].suggestedColor==uint16_t(0x349f));
    WatchData frame{}; frame.background=hub.snapshot();
    CHECK(hub.collect(0)==BackgroundUnchanged);
    // Another asset, not the same one rewritten: a new reference.
    p.icon=&otherIcon;
    CHECK(hub.collect(0)==BackgroundRestyled && frame.background.items[0].icon==&timerIcon);
    // Black is a colour, distinct from none.
    p.color=uint16_t(0);
    CHECK(hub.collect(0)==BackgroundRestyled && hub.snapshot().items[0].suggestedColor==uint16_t(0));
    p.color.reset();
    CHECK(hub.collect(0)==BackgroundRestyled && !hub.snapshot().items[0].suggestedColor);
    // Label and look together.
    p.text="12:35"; p.color=uint16_t(0x2e17);
    CHECK(hub.collect(0)==(BackgroundRelabeled|BackgroundRestyled));
    // An icon that cannot be drawn is no icon.
    static const IconBitmap noPixels{nullptr,3,2},noWidth{mask,0,2},noHeight{mask,3,-1};
    for (const IconBitmap* bad:{&noPixels,&noWidth,&noHeight}) {
        p.icon=bad; hub.collect(0);
        CHECK(hub.snapshot().items[0].icon==nullptr);
    }
    // An id the launcher does not know keeps the picture it brought.
    BackgroundInfoHub other; FakeProvider unknown(static_cast<LaunchTargetId>(42),"x");
    unknown.icon=&timerIcon; other.add(unknown); other.collect(0);
    CHECK(other.snapshot().items[0].icon==&timerIcon);
    // The stopwatch brings the launcher's own stopwatch mask and colour.
    StopwatchService service; StopwatchBackgroundInfo provider(service);
    service.start(0);
    BackgroundInfo out;
    CHECK(provider.sample(Second,out));
    CHECK(out.icon && out.icon==appIcon(IconId::Stopwatch) && out.suggestedColor==StopwatchAccent);
    BackgroundInfoHub stopwatch; stopwatch.add(provider); stopwatch.collect(Second);
    CHECK(stopwatch.collect(2*Second)==BackgroundRelabeled);     // the same asset each time
}

void deadlinesFollowTheShownItems() {
    BackgroundSnapshot s;
    CHECK(nextChange(s,leadingItems(s,2))==INT64_MAX);
    s.count=3;
    s.items[0].appId=LaunchTargetId::Stopwatch; s.items[0].nextChangeAt=500;
    s.items[1].appId=LaunchTargetId::External1; s.items[1].nextChangeAt=INT64_MAX;
    s.items[2].appId=LaunchTargetId::External2; s.items[2].nextChangeAt=100;
    const auto shown=leadingItems(s,2);
    CHECK(shown.count==2 && shown.ids[0]==LaunchTargetId::Stopwatch && shown.ids[1]==LaunchTargetId::External1);
    // The third item is not shown, so its earlier deadline does not count.
    CHECK(nextChange(s,shown)==500);
    CHECK(nextChange(s,leadingItems(s,4))==100);
    CHECK(nextChange(s,leadingItems(s,0))==INT64_MAX);
    // An id that has gone does not keep a deadline alive.
    BackgroundInterest stale; stale.ids[0]=LaunchTargetId::Settings; stale.count=1;
    CHECK(nextChange(s,stale)==INT64_MAX);
}

void stopwatchFormat() {
    TimeUs wait=0;
    CHECK(format(0,&wait)=="00:00" && wait==Second);
    CHECK(format(-5*Second,&wait)=="00:00" && wait==Second);
    CHECK(format(Second-1,&wait)=="00:00" && wait==1);
    CHECK(format(59*Second,&wait)=="00:59" && wait==Second);
    CHECK(format(59*Second+999999)=="00:59");                 // truncated, not rounded
    CHECK(format(Minute)=="01:00");
    CHECK(format(59*Minute+59*Second+400000,&wait)=="59:59" && wait==600000);
    // An hour switches the unit and the period together.
    CHECK(format(Hour,&wait)=="01:00" && wait==Minute);
    CHECK(format(Hour+59*Second,&wait)=="01:00" && wait==Second);
    CHECK(format(Hour+Minute+30*Second,&wait)=="01:01" && wait==30*Second);
    CHECK(format(9*Hour+59*Minute)=="09:59");
    CHECK(format(10*Hour)=="10:00");
    CHECK(format(99*Hour+59*Minute+59*Second)=="99:59");
    // No cap: the hours grow a digit instead of wrapping or saturating.
    CHECK(format(100*Hour)=="100:00");
    CHECK(format(12345*Hour+7*Minute)=="12345:07");
    // The longest the monotonic clock can hold still fits.
    const auto longest=format(INT64_MAX,&wait);
    CHECK(longest=="2562047788:00" && wait>0 && wait<=Minute);
}

void stopwatchProvider() {
    StopwatchService service;
    StopwatchBackgroundInfo provider(service);
    CHECK(provider.id()==LaunchTargetId::Stopwatch);
    BackgroundInfo out;
    CHECK(!provider.sample(0,out));                       // Reset: nothing
    service.start(1000);
    CHECK(provider.sample(1000,out) && std::string(out.label)=="00:00" && out.nextChangeAt==1000+Second);
    // Exactly on a boundary: the next one, not this one.
    CHECK(provider.sample(1000+5*Second,out) && std::string(out.label)=="00:05" && out.nextChangeAt==1000+6*Second);
    // A late frame shows the current label and waits for the boundary after
    // now, never replaying the ones it missed.
    CHECK(provider.sample(1000+8*Second+300000,out) && std::string(out.label)=="00:08");
    CHECK(out.nextChangeAt==1000+9*Second);
    service.stop(1000+10*Second+300000);
    CHECK(!provider.sample(1000+20*Second,out));          // Paused: nothing
    // Resumed: the boundaries follow the measurement, carrying its fraction.
    service.start(50*Second);
    CHECK(provider.sample(50*Second,out) && std::string(out.label)=="00:10");
    CHECK(out.nextChangeAt==50*Second+700000);
    CHECK(provider.sample(50*Second+700000,out) && std::string(out.label)=="00:11");
    // Into the hours, the boundaries become minutes of the measurement.
    const TimeUs hourAt=50*Second+Hour-10*Second-300000;
    CHECK(provider.sample(hourAt-1,out) && std::string(out.label)=="59:59" && out.nextChangeAt==hourAt);
    CHECK(provider.sample(hourAt,out) && std::string(out.label)=="01:00" && out.nextChangeAt==hourAt+Minute);
    CHECK(provider.sample(hourAt+90*Second,out) && std::string(out.label)=="01:01");
    CHECK(out.nextChangeAt==hourAt+2*Minute);
    service.reset();
    CHECK(!provider.sample(hourAt,out));                  // Reset: nothing
    // The deadline saturates instead of wrapping at the end of the clock.
    StopwatchService late; late.start(INT64_MAX-3*Second/2);
    StopwatchBackgroundInfo far(late);
    CHECK(far.sample(INT64_MAX-200000,out) && out.nextChangeAt==INT64_MAX);
}

void homeGetsTheLabelAfterTheScreenCloses() {
    StubHal hal; RecordingRender render;
    TimeService time; time.begin(hal);
    HomeDataSource data(hal,time);
    HostApplication application(hal,render,data,468,468); auto& runtime=application.runtime();
    CHECK(application.background().providers()==1 && application.background().rejected()==0);
    runtime.begin();
    hal.time=1000; runtime.step();
    CHECK(render.watch.background.count==0);             // Reset: nothing to show
    auto pressButton=[&](bool a) {
        (a ? hal.input.a : hal.input.b)=true; hal.time+=20000; runtime.step();
        (a ? hal.input.a : hal.input.b)=false; hal.time+=20000; runtime.step();
        hal.time+=200000; runtime.step();
    };
    auto goHome=[&] {
        hal.input.a=hal.input.b=true; hal.time+=10000; runtime.step();
        hal.time+=600000; runtime.step();
        hal.input={}; hal.time+=10000; runtime.step();
        CHECK(runtime.model().screen==ScreenId::Home);
    };
    pressButton(true);                                   // clock -> list, row 0
    pressButton(false);                                  // open the stopwatch
    pressButton(false);                                  // start
    CHECK(runtime.model().screen==ScreenId::Stopwatch);
    const TimeUs started=hal.time-200000;                // the release that started it
    CHECK(application.stopwatch().state()==StopwatchState::Running);
    // The stopwatch screen's 25ms frames do not sample the labels.
    const auto hidden=render.watch.background;
    for (int i=0;i<200;++i) { hal.time+=StopwatchFrameUs; runtime.step(); }
    CHECK(render.watch.background.count==hidden.count);
    CHECK(hidden.count==0);
    goHome();
    const auto& shown=render.watch.background;
    CHECK(shown.count==1 && shown.items[0].appId==LaunchTargetId::Stopwatch);
    const TimeUs elapsed=application.stopwatch().elapsed(hal.time);
    char expected[BackgroundLabelBytes];
    formatStopwatchBackground(elapsed,expected,sizeof(expected));
    CHECK(std::string(shown.items[0].label)==expected);
    CHECK(shown.items[0].nextChangeAt>hal.time && shown.items[0].nextChangeAt<=hal.time+Second);
    CHECK((shown.items[0].nextChangeAt-started)%Second==0);
    // The label's deadline does not drive the clock yet (task 10-2): a static
    // face draws nothing more for it.
    const int draws=render.draws;
    for (int i=0;i<20;++i) { hal.time+=100000; runtime.step(); }
    CHECK(render.draws==draws);
    // Setting the wall clock moves neither the label nor its deadline.
    CHECK(time.save({2026,1,1,0,0,0})==SaveResult::Saved);
    runtime.dataChanged(); hal.time+=10000; runtime.step();
    const TimeUs after=application.stopwatch().elapsed(hal.time);
    formatStopwatchBackground(after,expected,sizeof(expected));
    CHECK(render.watch.background.count==1 && std::string(render.watch.background.items[0].label)==expected);
    CHECK((render.watch.background.items[0].nextChangeAt-started)%Second==0);
    // Dark: the measurement goes on and nothing wakes for it.
    hal.time+=31*Second; runtime.step();
    CHECK(runtime.power().screenOff());
    const int dark=render.draws;
    application.background().invalidate();
    for (int i=0;i<600;++i) { hal.time+=Second; runtime.step(); }
    CHECK(render.draws==dark && application.background().pending());
    // Waking draws the newest label: over ten minutes have passed.
    hal.input.touching=true; hal.time+=10000; runtime.step();
    hal.input={}; hal.time+=10000; runtime.step();
    CHECK(render.draws>dark && !application.background().pending());
    formatStopwatchBackground(application.stopwatch().elapsed(hal.time),expected,sizeof(expected));
    CHECK(std::string(render.watch.background.items[0].label)==expected);
    CHECK(std::string(expected)>="10:");
    // A notification redraws a clock on screen at once.
    const int visible=render.draws;
    application.background().invalidate();
    hal.time+=1000; runtime.step();
    CHECK(render.draws==visible+1 && !application.background().pending());
    // Stopped from its screen: home shows nothing for it.
    pressButton(true); pressButton(false);
    CHECK(runtime.model().screen==ScreenId::Stopwatch);
    pressButton(false);                                  // stop
    CHECK(application.stopwatch().state()==StopwatchState::Paused);
    goHome();
    CHECK(render.watch.background.count==0);
}

void labelsWakeOnlyAFaceThatShowsThem() {
    // An RTC that cannot be trusted: the clock asks for no frames of its own,
    // and the label's deadlines still stand on their own.
    StubHal hal; hal.rtc={1900,1,1,0,0,0}; RecordingRender render;
    TimeService time; time.begin(hal);
    HomeDataSource data(hal,time);
    HostApplication application(hal,render,data,468,468); auto& runtime=application.runtime();
    runtime.begin();
    hal.time=1000; runtime.step();
    auto pressButton=[&](bool a) {
        (a ? hal.input.a : hal.input.b)=true; hal.time+=20000; runtime.step();
        (a ? hal.input.a : hal.input.b)=false; hal.time+=20000; runtime.step();
        hal.time+=200000; runtime.step();
    };
    auto framesOver=[&](int seconds) {
        const int before=render.draws;
        for (int i=0;i<seconds*10;++i) { hal.time+=100000; runtime.step(); }
        return render.draws-before;
    };
    pressButton(true); pressButton(false); pressButton(false);   // list, open, start
    CHECK(application.stopwatch().state()==StopwatchState::Running);
    hal.input.a=hal.input.b=true; hal.time+=10000; runtime.step();
    hal.time+=600000; runtime.step();
    hal.input={}; hal.time+=10000; runtime.step();
    CHECK(runtime.model().screen==ScreenId::Home && render.watch.background.count==1);
    CHECK(!render.watch.timeValid);
    // A face that leaves the label out is not woken for it.
    CHECK(framesOver(5)==0);
    // One that shows it is drawn once per change of the label, and each frame
    // carries the new text.
    render.showLabels=true; runtime.dataChanged(); hal.time+=1000; runtime.step();
    std::string last=render.watch.background.items[0].label;
    int changes=0;
    for (int i=0;i<50;++i) {
        const int before=render.draws;
        hal.time+=100000; runtime.step();
        if (render.draws!=before) {
            CHECK(std::string(render.watch.background.items[0].label)!=last);
            last=render.watch.background.items[0].label; ++changes;
        }
    }
    CHECK(changes==5);
    // Covered by the list: no frames for the label.
    pressButton(true);
    for (int i=0;i<30;++i) { hal.time+=10000; runtime.step(); }
    CHECK(runtime.model().screen==ScreenId::AppList && runtime.model().launcher.transition==1);
    CHECK(framesOver(5)==0);
    // Back home the label moves again at once, and dark it stops.
    hal.input.a=hal.input.b=true; hal.time+=10000; runtime.step();
    hal.time+=600000; runtime.step();
    hal.input={}; hal.time+=10000; runtime.step();
    CHECK(framesOver(5)==5);
    hal.time+=31*Second; runtime.step();
    CHECK(runtime.power().screenOff());
    CHECK(framesOver(5)==0);
}

int main() {
    collectsInRegistrationOrder();
    labelsAreCheckedAndOwned();
    deadlinesFollowTheShownItems();
    stopwatchFormat();
    stopwatchProvider();
    iconsAndColoursComeFromTheApp();
    homeGetsTheLabelAfterTheScreenCloses();
    labelsWakeOnlyAFaceThatShowsThem();
    std::cout << "background tests passed\n";
    return 0;
}
