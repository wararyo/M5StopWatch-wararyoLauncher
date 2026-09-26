#include "host/HostApplication.h"
#include "TestScreens.h"
#include "features/home/faces/DigitalLayout.h"
#include "host/LaunchRegistry.h"
#include "ui/list/ListLayout.h"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
struct FakeHal : Hal, RenderPort, DisplayDataSource {
    TimeUs time = 0, waited = 0, earlyWakeUs = 0;
    InputSnapshot input{};
    UsbState usb{};
    int draws = 0, sleeps = 0, wakes = 0, inputSamples = 0, usbSamples = 0, brightness = -1;
    FrameModel rendered{};
    TimeUs now() override { return time; }
    // Task 4 wires these up; the runtime tests only need them to compile.
    bool readRtc(CivilTime&) override { return false; }
    bool writeRtc(const CivilTime&) override { return false; }
    void setUtcClock(int64_t) override {}
    int64_t utcClockUs() override { return 0; }
    BatteryState sampleBattery() override { return {}; }
    void setBrightness(int level) override { brightness = level; }
    InputSnapshot sampleInput() override { ++inputSamples; return input; }
    UsbState sampleUsb() override { ++usbSamples; return usb; }
    // Panel commands and draws must never run while light sleep is allowed.
    bool lightSleep = false, panelWhileSleepAllowed = false;
    void setLightSleepAllowed(bool allowed) override { lightSleep = allowed; }
    void setScreenOff(bool off) override {
        off ? ++sleeps : ++wakes;
        panelWhileSleepAllowed = panelWhileSleepAllowed || lightSleep;
    }
    void invalidate() override {}
    void draw(const FrameModel& m, const WatchData&) override {
        ++draws; rendered = m;
        panelWhileSleepAllowed = panelWhileSleepAllowed || lightSleep;
    }
    TimeUs nextUpdate(TimeUs now, const WatchData& d) const override { return nextMinute(now,d); }
    void waitUs(TimeUs delay) override { waited = delay; time += delay - earlyWakeUs; }
    // A low-level interrupt: pending for as long as anything is pressed, or
    // once for an interrupt raised before anything reads as pressed.
    bool interrupt = false;
    bool inputPending() override {
        const bool pending = interrupt || input.a || input.b || input.touching;
        interrupt = false;
        return pending;
    }
};
void buttons() {
    InputController c;
    CHECK(!c.update(0, {true}).next);
    CHECK(c.update(1000, {}).next);
    CHECK(!c.update(2000, {}).next);
    c.update(3000, {false, true});
    CHECK(c.update(4000, {}).decide);
    c.update(5000, {true});
    c.update(100000, {true, true});
    CHECK(!c.update(699000, {true, true}).home);
    CHECK(c.update(700000, {true, true}).home);
    CHECK(!c.update(1500000, {true, true}).home);
    c.update(1600000, {true});
    c.update(1700000, {true, true});
    CHECK(!c.update(2400000, {true, true}).home);
    auto e = c.update(2500000, {});
    CHECK(!e.next && !e.decide);
    c.update(2600000, {false, true});
    CHECK(c.update(2700000, {}).decide);
    // Releasing either member resets continuous hold, but not short suppression.
    for (bool keepA : {false, true}) {
        InputController d;
        d.update(0, {true, true});
        e = d.update(500000, {keepA, !keepA});
        CHECK(!e.next && !e.decide);
        d.update(550000, {true, true});
        CHECK(!d.update(1149000, {true, true}).home);
        CHECK(d.update(1150000, {true, true}).home);
        e = d.update(1200000, {});
        CHECK(!e.next && !e.decide);
    }
    InputController shortChord;
    shortChord.update(0, {true, true});
    e = shortChord.update(599000, {});
    CHECK(!e.home && !e.next && !e.decide);
}
void touch() {
    InputController c;
    c.update(0, {false, false, true, 100, 100});
    CHECK(c.update(1000, {}).gesture == Gesture::Tap);
    c.update(2000, {false, false, true, 100, 100});
    CHECK(c.update(3000, {false, false, true, 100, 109}).gesture == Gesture::None);
    CHECK(c.update(4000, {false, false, true, 100, 110}).gesture == Gesture::DragStart);
    CHECK(c.update(5000, {false, false, true, 100, 120}).gesture == Gesture::DragMove);
    CHECK(c.update(6000, {}).gesture == Gesture::DragEnd);
    c.update(7000, {false, false, true, 100, 100}, true);
    CHECK(c.update(8000, {false, false, true, 100, 150}).gesture == Gesture::None);
    CHECK(c.update(9000, {}).gesture == Gesture::None);
    c.update(10000, {true, true, true, 100, 100});
    c.update(11000, {true, true, true, 100, 130});
    auto e = c.update(610000, {true, true, true, 100, 150});
    CHECK(e.home && e.gesture == Gesture::Cancel);
    CHECK(c.update(620000, {false, false, true, 100, 180}).gesture == Gesture::None);
    CHECK(c.update(630000, {}).gesture == Gesture::None);
}
void power() {
    PowerManager p;
    p.begin(0);
    p.update(29999999, false, false); CHECK(!p.screenOff());
    p.usb = {true, 5000, true};
    p.update(30000000, false, false); CHECK(p.screenOff());
    p.update(31000000, true, false); CHECK(!p.screenOff());
    p.update(61000000, true, false); CHECK(!p.screenOff());
    p.update(90999999, false, true); CHECK(p.state() == DisplayState::Active);
    p.update(91000000, false, true); CHECK(p.screenOff());
    constexpr TimeUs longTime = 5000000000000LL;
    p.begin(longTime);
    p.update(longTime + 30000000, false, false); CHECK(p.screenOff());
    CHECK(!UsbState{}.powered());
}
void releaseVelocity() {
    InputController c;
    CHECK(c.update(0,{false,false,true,100,300}).gesture==Gesture::TouchStart);
    for(int t=10000;t<=60000;t+=10000) c.update(t,{false,false,true,100,300-t/1000});
    auto e=c.update(70000,{});
    CHECK(e.gesture==Gesture::DragEnd && e.velocityY < -700 && e.velocityY > -1000);
    c.update(80000,{false,false,true,100,300});
    c.update(90000,{false,false,true,100,250});
    // Stale speed must not fling, including when no stationary sample arrived.
    CHECK(c.update(180000,{}).velocityY==0);
    c.update(200000,{false,false,true,100,300});
    c.update(210000,{false,false,true,100,250});
    for(int t=220000;t<=310000;t+=10000) c.update(t,{false,false,true,100,250});
    CHECK(c.update(320000,{}).velocityY==0);
    CHECK(c.update(330000,{false,false,true,100,300},true).gesture==Gesture::None);
}
void screens() {
    TestScreens s;
    Events e{}; e.next = true;
    CHECK(s.handle(e,0) && s.model().screen==ScreenId::AppList);
    CHECK(s.active()); s.update(180000); CHECK(!s.active() && s.model().launcher.transition==1);
    s.handle(e,200000); s.update(380000); CHECK(s.model().launcher.list.selection==1);
    e={}; e.decide=true; s.handle(e,400000);
    CHECK(s.model().screen==ScreenId::AppList && s.model().toast);
    s.update(1800000); CHECK(!s.model().toast);
    e={}; e.gesture=Gesture::DragStart; e.totalY=-40;
    s.handle(e,1900000); CHECK(s.active());
    e.home=e.next=e.decide=true;
    s.handle(e,2000000); CHECK(s.model().screen==ScreenId::Home && !s.active() && s.model().homeCount==1);
    e={}; e.gesture=Gesture::Tap; e.x=234; e.y=390;
    s.handle(e,2100000); CHECK(s.model().screen==ScreenId::AppList && s.model().launcher.list.selection==0);
}
void runtime() {
    FakeHal h;
    HostApplication application(h, h, h, 468, 468); auto& r=application.runtime(); r.begin(); r.step(); CHECK(h.draws == 1);
    // Idle: nothing is sampled between interrupts, so each wait runs to the
    // next deadline (the 1s USB sample) instead of the 10ms input period.
    const int idleSamples = h.inputSamples;
    for (int i = 0; i < 20; ++i) { r.wait(); CHECK(h.waited > 10000); r.step(); }
    CHECK(h.draws == 1); // USB sampling and idle waits do not redraw.
    CHECK(h.inputSamples == idleSamples && h.usbSamples >= 20);
    h.input.a = true; r.wait(); r.step();
    CHECK(h.inputSamples == idleSamples + 1); // The interrupt makes it read at once.
    r.wait(); CHECK(h.waited <= 10000); // Held: followed at the input period.
    h.input.a = false; r.wait(); r.step();
    CHECK(r.model().screen == ScreenId::AppList);
    h.time += 200000; r.step();
    const auto last = h.time;
    h.time = last + 30000000; r.step(); CHECK(r.power().screenOff() && h.sleeps == 1);
    const int draws = h.draws;
    h.time += 1000000; h.usb = {true, 5000, true}; r.step();
    CHECK(h.draws == draws && r.power().screenOff());
    h.time += 10000; h.input = {false, false, true, 100, 100}; r.step();
    CHECK(h.wakes == 1 && r.model().screen == ScreenId::AppList);
    CHECK(h.draws == draws + 1);
    h.time += 10000; h.input = {}; r.step(); CHECK(h.draws == draws + 1);
    h.time += 30000000; r.step(); CHECK(r.power().screenOff());
    h.time += 10000; h.input = {true, true}; r.step(); CHECK(!r.power().screenOff());
    h.time += 600000; r.step(); CHECK(r.model().screen == ScreenId::Home && r.model().homeCount == 1);
    h.time += 10000; h.input = {}; r.step(); CHECK(r.model().screen == ScreenId::Home);
    // A wake press still acts normally on release.
    h.time += 30000000; r.step();
    h.time += 10000; h.input.a = true; r.step();
    h.time += 10000; h.input.a = false; r.step(); CHECK(r.model().screen == ScreenId::AppList);
    h.time += 400000; r.step(); r.wait(); CHECK(h.waited > 10000); // Released and settled.
    CHECK(HostRuntime::waitDelay(100000, 90000) == 1000);
    CHECK(HostRuntime::waitDelay(100000, 105000) == 5000);
    CHECK(LaunchRegistry.size() == 5 && LaunchRegistry[2].slot == 1 && LaunchRegistry[4].slot == 3);
    for (const auto& entry : LaunchRegistry) CHECK(entry.name && entry.name[0]);
}
void lightSleep() {
    // Work 8-5: light sleep only with the panel asleep and a VBUS reading that
    // says no USB power, and never while a panel command or draw runs.
    FakeHal h;
    HostApplication application(h, h, h, 468, 468); auto& r=application.runtime(); r.begin(); r.step();
    CHECK(!h.lightSleep); // Screen on.
    h.time += 31000000; r.step();
    CHECK(r.power().screenOff() && !h.lightSleep); // VBUS not read successfully yet.
    h.usb = {true, 5000, true};
    h.time += 1000000; r.step(); CHECK(!h.lightSleep); // USB power.
    h.usb = {true, 0, false};
    h.time += 1000000; r.step(); CHECK(h.lightSleep); // Battery, panel asleep.
    h.usb = {false, 0, false};
    h.time += 1000000; r.step(); CHECK(!h.lightSleep); // An unanswered read forbids it.
    h.usb = {true, 0, false};
    h.time += 1000000; r.step(); CHECK(h.lightSleep);
    // A touch wakes the panel: sleep is forbidden before the panel command.
    h.time += 10000; h.input = {false, false, true, 100, 100}; r.step();
    CHECK(!r.power().screenOff() && !h.lightSleep && h.wakes == 1);
    h.time += 10000; h.input = {}; r.step();
    // Falling asleep again allows it only after the panel went off.
    h.time += 31000000; r.step(); CHECK(r.power().screenOff() && h.lightSleep);
    // USB plugged in while asleep: forbidden at the next VBUS sample.
    h.usb = {true, 5000, true};
    h.time += 1000000; r.step(); CHECK(!h.lightSleep);
    CHECK(!h.panelWhileSleepAllowed);
}
void interrupts() {
    // Work 8-4: the touch controller raises INT before its first report is
    // readable, so an interrupt that reads nothing still starts a short stretch
    // of polling at the input period, and interrupts never bring a read
    // forward while one is being followed.
    FakeHal h;
    HostApplication application(h, h, h, 468, 468); auto& r=application.runtime(); r.begin(); r.step();
    r.wait(); r.step();
    const int idle = h.inputSamples;
    h.interrupt = true; r.step();
    CHECK(h.inputSamples == idle + 1);
    h.time += 3000; h.interrupt = true; r.step();
    CHECK(h.inputSamples == idle + 1); // Not brought forward.
    h.time += 7000; r.step(); CHECK(h.inputSamples == idle + 2);
    r.wait(); CHECK(h.waited <= 10000); // Still following although nothing was read.
    for (int i = 0; i < 10; ++i) { h.time += 10000; r.step(); }
    r.wait(); CHECK(h.waited > 10000); // The stretch ended: back to deadlines.
}
void overload() {
    // vTaskDelay counts tick boundaries, so elapsed time can be shorter than
    // the requested tick count by nearly one tick. Exercise that phase error.
    for (TimeUs early : {0, 1, 500, 999}) {
        FakeHal h; h.earlyWakeUs = early;
        HostApplication application(h, h, h, 468, 468); auto& r=application.runtime(); r.begin(); r.step();
        auto cycle = [&] {
            h.time += 40000;
            r.wait(); CHECK(h.waited >= 1000);
            r.step();
        };
        // Let an early wake happen before pressing, as in the device report.
        cycle();
        h.input.a = true;
        for (int i = 0; i < 5; ++i) cycle();
        h.input = {};
        for (int i = 0; i < 5; ++i) cycle();
        CHECK(r.model().screen == ScreenId::AppList);
        h.input = {true, true};
        const int heldSamples = h.inputSamples;
        for (int i = 0; i < 20; ++i) cycle();
        CHECK(r.model().homeCount == 1);
        // Overruns never postpone a held input: every cycle still reads it.
        CHECK(h.inputSamples - heldSamples == 20);
        h.input = {};
        for (int i = 0; i < 800; ++i) cycle();
        CHECK(r.power().screenOff());
        h.input = {false, false, true, 100, 100};
        for (int i = 0; i < 3; ++i) cycle();
        CHECK(!r.power().screenOff());
        h.input = {};
        for (int i = 0; i < 3; ++i) cycle();
        CHECK(r.model().screen == ScreenId::Home); // Wake touch was consumed.
        CHECK(h.usbSamples > 30);
    }
}

// Work 10-2: a still touch on the resting clock becomes one long press.
Gesture held(InputController& c, TimeUs at, int x, int y, bool home = true) {
    return c.update(at, {false, false, true, x, y}, false, home).gesture;
}
Gesture lifted(InputController& c, TimeUs at, bool home = true) { return c.update(at, {}, false, home).gesture; }
void longPress() {
    constexpr TimeUs L = InputController::LongPressUs;
    InputController c;
    // 599ms is not yet, 600ms is, and only once however long it is held.
    CHECK(held(c, 0, 100, 100) == Gesture::TouchStart);
    CHECK(held(c, L - 1000, 100, 100) == Gesture::None);
    CHECK(held(c, L, 100, 100) == Gesture::LongPress);
    CHECK(held(c, L + 10000, 100, 100) == Gesture::None);
    CHECK(held(c, 3 * L, 100, 100) == Gesture::None);
    // Spent: moving now is no drag, and letting go is no tap.
    CHECK(held(c, 3 * L + 10000, 100, 200) == Gesture::None);
    CHECK(lifted(c, 3 * L + 20000) == Gesture::None);
    // The next touch starts afresh; 601ms (the next sample) fires it too.
    TimeUs t = 5000000;
    held(c, t, 100, 100); CHECK(held(c, t + L + 1000, 100, 100) == Gesture::LongPress);
    lifted(c, t + L + 11000);
    // A finger that trembles within the drag threshold still holds still.
    t = 7000000;
    held(c, t, 100, 100); held(c, t + 100000, 104, 97); held(c, t + 300000, 109, 100);
    CHECK(held(c, t + L, 108, 105) == Gesture::LongPress);
    lifted(c, t + L + 10000);
    // Past the threshold it is a drag for good, back at the start or not.
    t = 9000000;
    held(c, t, 100, 100);
    CHECK(held(c, t + 100000, 100, 110) == Gesture::DragStart);
    CHECK(held(c, t + 200000, 100, 100) == Gesture::DragMove);
    CHECK(held(c, t + L + 100000, 100, 100) == Gesture::None);
    CHECK(lifted(c, t + L + 110000) == Gesture::DragEnd);
    // Movement and time on the same sample: the drag wins.
    t = 11000000;
    held(c, t, 100, 100);
    CHECK(held(c, t + L, 100, 110) == Gesture::DragStart);
    lifted(c, t + L + 10000);
    // Home on the sample that reaches 600ms: home, and no long press.
    t = 13000000;
    c.update(t, {true, true, true, 100, 100}, false, true);
    auto e = c.update(t + L, {true, true, true, 100, 100}, false, true);
    CHECK(e.home && e.gesture == Gesture::Cancel);
    CHECK(c.update(t + L + 10000, {false, false, true, 100, 100}, false, true).gesture == Gesture::None);
    c.update(t + L + 20000, {}, false, true);
    // A touch that wakes the panel is consumed whole.
    t = 15000000;
    c.update(t, {false, false, true, 100, 100}, true, true);
    CHECK(held(c, t + L, 100, 100) == Gesture::None && lifted(c, t + L + 10000) == Gesture::None);
    // Elsewhere (home not at rest when the touch began) a long hold stays a
    // tap, and home arriving under the finger does not arm it.
    t = 17000000;
    CHECK(held(c, t, 100, 100, false) == Gesture::TouchStart);
    CHECK(held(c, t + L, 100, 100, true) == Gesture::None);
    CHECK(held(c, t + 3 * L, 100, 100, true) == Gesture::None);
    CHECK(lifted(c, t + 3 * L + 10000, true) == Gesture::Tap);
    // The clock stops being at rest under a still touch (a button opened the
    // list): the touch is consumed, no long press and no tap.
    t = 20000000;
    held(c, t, 100, 100);
    CHECK(held(c, t + 100000, 100, 100, false) == Gesture::None);
    CHECK(held(c, t + L, 100, 100, false) == Gesture::None);
    CHECK(lifted(c, t + L + 10000, false) == Gesture::None);
    // ...even when the release is the first sample that sees it.
    t = 22000000;
    held(c, t, 100, 100);
    CHECK(lifted(c, t + 100000, false) == Gesture::None);
    // A pull up is a drag: it moves the clock itself and carries on.
    t = 24000000;
    held(c, t, 100, 300);
    CHECK(held(c, t + 50000, 100, 280) == Gesture::DragStart);
    CHECK(held(c, t + 60000, 100, 200, false) == Gesture::DragMove);
    CHECK(lifted(c, t + 70000, false) == Gesture::DragEnd);
}
// The runtime with Digital's behaviour behind the clock (its drawing aside).
struct FaceHal : FakeHal, HomeControlPort {
    DigitalControl digital;
    int faceEvents = 0;
    HomeOutcome handle(const HomeEvent& e) override { ++faceEvents; return digital.handle(e, {468, 468}); }
    WatchData sample(TimeUs now) override {
        WatchData d; d.timeValid = true;
        d.localTime.tm_hour = 9; d.localTime.tm_min = 41;
        d.localTime.tm_sec = int(now / 1000000 % 60); d.subsecondUs = now % 1000000;
        return d;
    }
    TimeUs nextUpdate(TimeUs now, const WatchData& d) const override { return digital.nextUpdate(now, d); }
};
void homeGestures() {
    FaceHal h;
    HostApplication application(h, h, h, 468, 468); application.bindHome(h);
    auto& r = application.runtime(); r.begin(); r.step();
    // Frames drawn over whole seconds of idling.
    auto framesOver = [&](int seconds) {
        const int before = h.draws;
        for (int i = 0; i < seconds; ++i) { h.time += 1000000; r.step(); }
        return h.draws - before;
    };
    CHECK(framesOver(5) == 0); // Minutes: nothing due within them.
    // Held still on the clock: the long press arrives while the finger rests
    // (followed at the input period), the seconds show, and the display now
    // waits for the next second.
    const int draws = h.draws;
    h.time = 10000000; h.input = {false, false, true, 234, 380}; r.step(); // on APPS
    const TimeUs pressed = h.time;
    for (int i = 0; i < 70 && h.faceEvents == 0; ++i) { h.time += 10000; r.step(); }
    CHECK(h.faceEvents == 1 && h.digital.variant() == DigitalVariant::HourMinuteSecond);
    CHECK(h.draws == draws + 1);
    CHECK(h.time - pressed >= InputController::LongPressUs && h.time - pressed < InputController::LongPressUs + 20000);
    // Released on APPS: the press was spent, so the list does not open.
    h.time += 10000; h.input = {}; r.step();
    CHECK(r.model().screen == ScreenId::Home && h.faceEvents == 1);
    h.time += 200000; r.step();
    CHECK(framesOver(5) == 5);              // One frame per second, no more.
    // A tap on APPS asks for the list, which the system opens with its slide.
    h.time += 10000; h.input = {false, false, true, 234, 380}; r.step();
    h.time += 10000; h.input = {}; r.step();
    CHECK(h.faceEvents == 2 && r.model().screen == ScreenId::AppList);
    // Under the list the seconds wake nothing: the covered clock has no deadline.
    for (int i = 0; i < 40; ++i) { h.time += 10000; r.step(); }
    CHECK(r.model().launcher.transition == 1);
    CHECK(framesOver(5) == 0);
    // A long hold on the list is not the clock's, and stays the list's tap.
    h.time += 10000; h.input = {false, false, true, 234, 60}; r.step();
    for (int i = 0; i < 100; ++i) { h.time += 10000; r.step(); }
    h.time += 10000; h.input = {}; r.step();
    CHECK(h.faceEvents == 2);
    // Home, then back to minutes with another long press.
    h.time += 10000; h.input = {true, true}; r.step();
    h.time += 600000; r.step();
    h.time += 10000; h.input = {}; r.step();
    CHECK(r.model().screen == ScreenId::Home);
    h.time += 10000; h.input = {false, false, true, 100, 234}; r.step();
    for (int i = 0; i < 70; ++i) { h.time += 10000; r.step(); }
    h.time += 10000; h.input = {}; r.step();
    CHECK(h.faceEvents == 3 && h.digital.variant() == DigitalVariant::HourMinute);
    CHECK(r.model().screen == ScreenId::Home);
    h.time += 200000; r.step();
    CHECK(framesOver(5) == 0);              // The second period is gone.
}
int main() {
    buttons(); touch(); releaseVelocity(); power(); screens(); runtime(); interrupts(); lightSleep();
    overload(); longPress(); homeGestures();
    std::cout << "PASS: buttons, touch, power, screens, runtime/registry, interrupts, light sleep, overload/early-wake, "
                 "long press, home gestures\n";
}
