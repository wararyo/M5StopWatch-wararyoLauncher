#include "app/Application.h"
#include "TestScreens.h"
#include "app/AppRegistry.h"
#include "features/stopwatch/StopwatchScreen.h"
#include <cstdlib>
#include <iostream>
#include <string>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
struct StubHal : Hal {
    TimeUs time=0;
    InputSnapshot input{};
    TimeUs now() override { return time; }
    InputSnapshot sampleInput() override { return input; }
    UsbState sampleUsb() override { return {}; }
    void setScreenOff(bool) override {}
    void waitUs(TimeUs) override {}
    // A low-level interrupt: pending for as long as anything is pressed.
    bool inputPending() override { return input.a || input.b || input.touching; }
    bool readRtc(CivilTime&) override { return false; }
    bool writeRtc(const CivilTime&) override { return false; }
    void setUtcClock(int64_t) override {}
    int64_t utcClockUs() override { return 0; }
    BatteryState sampleBattery() override { return {}; }
    void setBrightness(int) override {}
};
struct StubRender : RenderPort {
    int draws=0;
    FrameModel last{};
    void invalidate() override {}
    void draw(const FrameModel& m,const WatchData&) override { ++draws; last=m; }
    TimeUs nextUpdate(TimeUs,const WatchData&) const override { return INT64_MAX; }
};
Events press(bool next) { Events e{}; e.next=next; e.decide=!next; return e; }
Events tap(int x,int y) { Events e{}; e.gesture=Gesture::Tap; e.x=x; e.y=y; return e; }
Events home() { Events e{}; e.home=true; return e; }
std::string shown(TimeUs elapsed) {
    char clock[16],fraction[8];
    formatStopwatch(elapsed,clock,sizeof(clock),fraction,sizeof(fraction));
    return std::string(clock)+fraction;
}
constexpr TimeUs Second=1000000;

void accumulatesAcrossPauses() {
    StopwatchService sw;
    CHECK(sw.state()==StopwatchState::Reset);
    CHECK(sw.elapsed(5*Second)==0);              // Nothing runs before start.
    sw.start(1000);
    CHECK(sw.state()==StopwatchState::Running);
    CHECK(sw.elapsed(1000+3*Second)==3*Second);
    sw.stop(1000+3*Second);
    CHECK(sw.state()==StopwatchState::Paused);
    // Paused time does not count, however long the screen stays away.
    CHECK(sw.elapsed(1000+90*Second)==3*Second);
    sw.start(1000+90*Second);
    CHECK(sw.elapsed(1000+92*Second)==5*Second);
    // A repeat of the same press is ignored rather than re-based.
    sw.start(1000+95*Second);
    CHECK(sw.elapsed(1000+92*Second)==5*Second);
    sw.stop(1000+92*Second);
    sw.stop(1000+99*Second);
    CHECK(sw.elapsed(1000+99*Second)==5*Second);
    sw.reset();
    CHECK(sw.state()==StopwatchState::Reset);
    CHECK(sw.elapsed(1000+99*Second)==0);
}

void lapsOnlyWhileRunning() {
    StopwatchService sw;
    sw.lap(Second);
    CHECK(sw.lapRows()==0 && sw.lapCount()==0);
    sw.start(0);
    for (int i=1;i<=5;++i) sw.lap(TimeUs(i)*Second);
    // Numbering keeps counting; only the rows that fit are kept, newest first.
    CHECK(sw.lapCount()==5);
    CHECK(sw.lapRows()==StopwatchLapRows);
    CHECK(sw.lapNumber(0)==5 && sw.lapTime(0)==5*Second);
    CHECK(sw.lapNumber(1)==4 && sw.lapTime(1)==4*Second);
    CHECK(sw.lapNumber(2)==3 && sw.lapTime(2)==3*Second);
    CHECK(sw.lapNumber(StopwatchLapRows)==0 && sw.lapTime(-1)==0);
    sw.stop(6*Second);
    sw.lap(7*Second);
    CHECK(sw.lapCount()==5);                     // Paused takes no laps.
    sw.reset();
    CHECK(sw.lapRows()==0 && sw.lapCount()==0 && sw.lapNumber(0)==0);
}

void formatsAndCapsTheDisplay() {
    CHECK(shown(0)=="00:00:00.00");
    CHECK(shown(9999)=="00:00:00.00");           // Truncates, never rounds up.
    CHECK(shown(10000)=="00:00:00.01");
    CHECK(shown(59*Second+990000)=="00:00:59.99");
    CHECK(shown(60*Second)=="00:01:00.00");
    CHECK(shown(3600*Second)=="01:00:00.00");
    CHECK(shown(StopwatchDisplayCapUs)=="99:59:59.99");
    // Past the cap the display holds still; the measurement does not.
    CHECK(shown(StopwatchDisplayCapUs+Second)=="99:59:59.99");
    CHECK(shown(500LL*3600*Second)=="99:59:59.99");
    StopwatchService sw;
    sw.start(0);
    CHECK(sw.elapsed(StopwatchDisplayCapUs+60*Second)==StopwatchDisplayCapUs+60*Second);
}

void buttonsFollowTheState() {
    StopwatchService sw; StopwatchScreen screen;
    screen.bind(&sw); screen.resize(468,468);
    TimeUs now=1000;
    screen.enter(now);
    CHECK(screen.model().state==StopwatchState::Reset);
    // A is dead in Reset: nothing to lap and nothing to clear.
    CHECK(!screen.handle(press(true),now).changed);
    CHECK(sw.state()==StopwatchState::Reset);
    CHECK(screen.handle(press(false),now).changed);
    CHECK(sw.state()==StopwatchState::Running);
    now+=2*Second;
    screen.handle(press(true),now);              // A laps while running
    CHECK(sw.lapCount()==1 && sw.lapTime(0)==2*Second);
    CHECK(screen.model().rows==1 && screen.model().lapNumber[0]==1);
    screen.handle(press(false),now);             // B stops
    CHECK(sw.state()==StopwatchState::Paused);
    screen.handle(press(false),now);             // B resumes
    CHECK(sw.state()==StopwatchState::Running);
    screen.handle(press(false),now);
    screen.handle(press(true),now);              // A resets while paused
    CHECK(sw.state()==StopwatchState::Reset);
    CHECK(sw.lapCount()==0 && screen.model().rows==0);
    // The screen never offers a way back to the list.
    CHECK(!screen.handle(press(true),now).leave);
    CHECK(!screen.handle(press(false),now).leave);
}

void touchHitsTheSameButtons() {
    StopwatchService sw; StopwatchScreen screen;
    screen.bind(&sw); screen.resize(468,468);
    TimeUs now=0;
    screen.enter(now);
    FrameModel m; m.viewport.width=468; m.viewport.height=468; m.screen=ScreenId::Stopwatch;
    const auto left=stopwatchButtonBox(m.viewport,0),right=stopwatchButtonBox(m.viewport,1);
    screen.handle(tap(right.x+right.w/2,right.y+right.h/2),now);
    CHECK(sw.state()==StopwatchState::Running);
    now+=Second;
    screen.handle(tap(left.x+left.w/2,left.y+left.h/2),now);
    CHECK(sw.lapCount()==1);
    // A tap on the panel is not a button.
    CHECK(!screen.handle(tap(m.viewport.width/2,stopwatchPanelBox(m.viewport).y+10),now).changed);
    CHECK(sw.lapCount()==1);
    CHECK(sw.state()==StopwatchState::Running);
}

void framesOnlyWhileRunning() {
    StopwatchService sw; StopwatchScreen screen;
    screen.bind(&sw); screen.resize(468,468);
    TimeUs now=1000;
    screen.enter(now);
    CHECK(screen.nextUpdate()==INT64_MAX);       // Reset is a static picture.
    screen.handle(press(false),now);
    CHECK(screen.nextUpdate()==now+StopwatchFrameUs);
    now+=StopwatchFrameUs;
    CHECK(screen.tick(now));
    CHECK(screen.nextUpdate()==now+StopwatchFrameUs);
    CHECK(screen.model().elapsedUs==StopwatchFrameUs);
    screen.handle(press(false),now);             // stop
    CHECK(screen.nextUpdate()==INT64_MAX);
    // Leaving drops the deadline without touching the measurement.
    screen.handle(press(false),now);
    screen.exit();
    CHECK(screen.nextUpdate()==INT64_MAX);
    CHECK(sw.state()==StopwatchState::Running);
    now+=10*Second;
    screen.enter(now);
    CHECK(screen.model().elapsedUs==StopwatchFrameUs+10*Second);
    CHECK(screen.nextUpdate()==now+StopwatchFrameUs);
}

void homeKeepsMeasuringAndTheListOpensIt() {
    TestScreens s; TimeUs now=0;
    s.handle(home(),now); now+=1000;
    s.handle(press(true),now); now+=200000; s.update(now);   // clock -> list
    CHECK(AppRegistry[s.model().launcher.list.selection].id==AppId::Stopwatch);
    s.handle(press(false),now); now+=1000;
    CHECK(s.model().screen==ScreenId::Stopwatch);
    s.handle(press(false),now); now+=1000;
    CHECK(s.stopwatch.state()==StopwatchState::Running);
    // Home lands on the clock, and the measurement is not the screen to stop.
    CHECK(s.handle(home(),now));
    CHECK(s.model().screen==ScreenId::Home);
    CHECK(s.stopwatch.state()==StopwatchState::Running);
    // A static clock asks for no frames of its own.
    CHECK(s.nextUpdate()==INT64_MAX);
    now+=5*Second;
    s.handle(press(true),now); now+=200000; s.update(now);
    s.handle(press(false),now); now+=1000;
    const auto m=s.model();
    CHECK(m.screen==ScreenId::Stopwatch);
    CHECK(m.stopwatch.state==StopwatchState::Running);
    CHECK(m.stopwatch.elapsedUs>=5*Second);
}

void managerDrivesTheScreensOwnDeadline() {
    TestScreens s; TimeUs now=0;
    s.handle(press(true),now); now+=200000; s.update(now);
    s.handle(press(false),now);                  // open
    s.handle(press(false),now);                  // start
    const auto due=s.nextUpdate();
    CHECK(due==now+StopwatchFrameUs);
    CHECK(!s.update(now));                       // Not due yet: no repaint.
    now=due;
    CHECK(s.update(now));                        // Due: the screen re-sampled.
    CHECK(s.model().stopwatch.elapsedUs==StopwatchFrameUs);
    CHECK(s.nextUpdate()==now+StopwatchFrameUs);
    // Waking a little late keeps the cadence on the deadlines (work 8-4)...
    const auto next=s.nextUpdate();
    now=next+700; CHECK(s.update(now));
    CHECK(s.nextUpdate()==next+StopwatchFrameUs);
    // ...but a whole lost period is not replayed: it restarts from now.
    now=s.nextUpdate()+StopwatchFrameUs+1000; CHECK(s.update(now));
    CHECK(s.nextUpdate()==now+StopwatchFrameUs);
}

void runtimeStopsFramesWhileBlanked() {
    StubHal hal; StubRender render; DisplayDataSource data;
    Application application(hal,render,data,468,468); auto& runtime=application.runtime();
    runtime.begin();
    hal.time=1000; runtime.step();
    auto pressButton=[&](bool a) {
        (a ? hal.input.a : hal.input.b)=true; hal.time+=20000; runtime.step();
        (a ? hal.input.a : hal.input.b)=false; hal.time+=20000; runtime.step();
        hal.time+=200000; runtime.step();
    };
    pressButton(true);                           // clock -> list, row 0
    pressButton(false);                          // open the stopwatch
    CHECK(runtime.model().screen==ScreenId::Stopwatch);
    pressButton(false);                          // start
    CHECK(runtime.model().stopwatch.state==StopwatchState::Running);
    const int running=render.draws;
    for (int i=0;i<10;++i) { hal.time+=StopwatchFrameUs; runtime.step(); }
    CHECK(render.draws>=running+10);
    // A running measurement is not activity, so the panel still blanks on time
    // (plan.md 7.1).
    hal.time+=31*Second; runtime.step();
    CHECK(runtime.power().screenOff());
    const int blanked=render.draws;
    for (int i=0;i<40;++i) { hal.time+=StopwatchFrameUs; runtime.step(); }
    CHECK(render.draws==blanked);
    // Waking re-samples: the measurement kept the time the display did not.
    hal.input.touching=true; hal.time+=20000; runtime.step();
    CHECK(!runtime.power().screenOff());
    CHECK(render.draws>blanked);
    CHECK(render.last.screen==ScreenId::Stopwatch);
    CHECK(render.last.stopwatch.state==StopwatchState::Running);
    CHECK(render.last.stopwatch.elapsedUs>=31*Second);
}
}
int main() {
    accumulatesAcrossPauses();
    lapsOnlyWhileRunning();
    formatsAndCapsTheDisplay();
    buttonsFollowTheState();
    touchHitsTheSameButtons();
    framesOnlyWhileRunning();
    homeKeepsMeasuringAndTheListOpensIt();
    managerDrivesTheScreensOwnDeadline();
    runtimeStopsFramesWhileBlanked();
    std::cout << "stopwatch tests passed\n";
    return 0;
}
