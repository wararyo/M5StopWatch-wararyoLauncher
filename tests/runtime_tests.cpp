#include "app/Application.h"
#include "TestScreens.h"
#include "app/AppRegistry.h"
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
    void setScreenOff(bool off) override { off ? ++sleeps : ++wakes; }
    void invalidate() override {}
    void draw(const FrameModel& m, const WatchData&) override { ++draws; rendered = m; }
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
    Application application(h, h, h, 468, 468); auto& r=application.runtime(); r.begin(); r.step(); CHECK(h.draws == 1);
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
    CHECK(AppRuntime::waitDelay(100000, 90000) == 1000);
    CHECK(AppRuntime::waitDelay(100000, 105000) == 5000);
    CHECK(AppRegistry.size() == 5 && AppRegistry[2].slot == 1 && AppRegistry[4].slot == 3);
    for (const auto& entry : AppRegistry) CHECK(entry.name && entry.name[0]);
}
void interrupts() {
    // Work 8-4: the touch controller raises INT before its first report is
    // readable, so an interrupt that reads nothing still starts a short stretch
    // of polling at the input period, and interrupts never bring a read
    // forward while one is being followed.
    FakeHal h;
    Application application(h, h, h, 468, 468); auto& r=application.runtime(); r.begin(); r.step();
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
        Application application(h, h, h, 468, 468); auto& r=application.runtime(); r.begin(); r.step();
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
int main() {
    buttons(); touch(); releaseVelocity(); power(); screens(); runtime(); interrupts();
    overload();
    std::cout << "PASS: buttons, touch, power, screens, runtime/registry, interrupts, overload/early-wake\n";
}
