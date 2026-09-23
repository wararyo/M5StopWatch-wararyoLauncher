#include "app/AppRuntime.h"
#include "app/AppRegistry.h"
#include "multifirm/FakeSlotService.h"
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
    ScreenModel last{};
    void invalidate() override {}
    void draw(const ScreenModel& m,const WatchData&) override { ++draws; last=m; }
    TimeUs nextUpdate(TimeUs,const WatchData&) const override { return INT64_MAX; }
};
Events press(bool next) { Events e{}; e.next=next; e.decide=!next; return e; }
Events tap(int x,int y) { Events e{}; e.gesture=Gesture::Tap; e.x=x; e.y=y; return e; }
Events home() { Events e{}; e.home=true; return e; }

// Drives the manager from the clock to the given app list row and opens it.
void openRow(ScreenManager& s,TimeUs& now,AppId id) {
    s.handle(home(),now); now+=1000;
    const Events next=press(true);
    s.handle(next,now); now+=200000; s.update(now);        // clock -> list
    while (AppRegistry[s.model().list.selection].id!=id) { s.handle(next,now); now+=200000; s.update(now); }
    s.handle(press(false),now); now+=1000;
}

SlotCatalog scanning() {
    SlotCatalog c;
    for (auto& entry:c.slots) entry.status=SlotStatus::Scanning;
    return c;
}

void listShowsNamesAndDimming() {
    ScreenManager s;
    s.setSlots(scanning());
    auto m=s.model();
    // Nothing is launchable while the scan runs, and no slot lends its name.
    for (int i=0;i<int(AppRegistry.size());++i) {
        const bool external=AppRegistry[i].kind==TargetKind::External;
        CHECK(m.rowDimmed[i]==external);
        CHECK(m.names[i]==nullptr);
    }
    FakeSlotService slots;
    slots.catalog=scanning();
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    slots.set(2,SlotStatus::Empty);
    slots.set(3,SlotStatus::Invalid,nullptr,nullptr,0x105);
    s.setSlots(slots.catalog);
    m=s.model();
    CHECK(std::string(m.names[2])=="KantanPlay");
    CHECK(!m.rowDimmed[2]);
    CHECK(m.names[3]==nullptr && m.rowDimmed[3]);
    CHECK(m.names[4]==nullptr && m.rowDimmed[4]);
    // Built-in rows are never dimmed by a slot result.
    CHECK(!m.rowDimmed[0] && !m.rowDimmed[1]);
}

void unsupportedLayoutDisablesEveryExternalRow() {
    ScreenManager s;
    FakeSlotService slots;
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    slots.setUnsupported();
    s.setSlots(slots.catalog);
    const auto m=s.model();
    for (int i=0;i<int(AppRegistry.size());++i)
        if (AppRegistry[i].kind==TargetKind::External) {
            CHECK(m.rowDimmed[i]);
            CHECK(m.names[i]==nullptr);
        }
}

void launchableRowStartsWithoutConfirmation() {
    FakeSlotService slots;
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    ScreenManager s; TimeUs now=0;
    s.bindSlots(&slots); s.setSlots(slots.catalog);
    // Deciding the row IS the final decision: no confirmation, no second press.
    openRow(s,now,AppId::External1);
    const auto m=s.model();
    CHECK(m.screen==ScreenId::External);
    CHECK(m.external.slot==1 && m.external.phase==ExternalPhase::BootCommitting);
    CHECK(externalButtonCount(m.external)==0);
    // Still nothing called until the committing frame has been painted.
    CHECK(slots.bootRequests==0);
    CHECK(s.commitPendingBoot(now));
    CHECK(slots.bootRequests==1 && slots.bootSlot==1);
}

void unusableSlotsOpenTheScreenInstead() {
    FakeSlotService slots;
    slots.catalog=scanning();
    slots.set(2,SlotStatus::Empty);
    slots.set(3,SlotStatus::ReadError,nullptr,nullptr,0x102);
    ScreenManager s; TimeUs now=0;
    s.bindSlots(&slots); s.setSlots(slots.catalog);
    // An unusable slot explains itself and only offers the way back.
    openRow(s,now,AppId::External2);
    auto m=s.model();
    CHECK(m.screen==ScreenId::External && m.external.status==SlotStatus::Empty);
    CHECK(m.external.phase==ExternalPhase::Browsing);
    CHECK(m.external.name==nullptr);
    CHECK(externalButtonCount(m.external)==1);
    openRow(s,now,AppId::External3);
    m=s.model();
    CHECK(m.external.status==SlotStatus::ReadError && m.external.error==0x102);
    CHECK(m.external.phase==ExternalPhase::Browsing);
    CHECK(externalButtonCount(m.external)==1);
    // A slot still being verified is not launchable either.
    openRow(s,now,AppId::External1);
    m=s.model();
    CHECK(m.external.status==SlotStatus::Scanning);
    CHECK(m.external.phase==ExternalPhase::Browsing);
    // Nothing here starts a boot, and the way back lands on the row it came from.
    CHECK(slots.bootRequests==0);
    CHECK(!s.commitPendingBoot(now));
    s.handle(press(false),now); now+=1000;
    m=s.model();
    CHECK(m.screen==ScreenId::AppList);
    CHECK(AppRegistry[m.list.selection].id==AppId::External1);
    // Every row now opens something; nothing falls through to the notice.
    openRow(s,now,AppId::Stopwatch);
    CHECK(s.model().screen==ScreenId::Stopwatch);
}

void commitSuppressesInputAndRecovers() {
    FakeSlotService slots;
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    slots.bootMessage="ESP_ERR_IMAGE_INVALID";
    ScreenManager s; TimeUs now=0;
    s.bindSlots(&slots); s.setSlots(slots.catalog);
    openRow(s,now,AppId::External1);               // one press: decide == launch
    auto m=s.model();
    CHECK(m.external.phase==ExternalPhase::BootCommitting);
    CHECK(externalButtonCount(m.external)==0);
    // Nothing reaches the screen while the commit is outstanding, home least of
    // all: the API has not been called yet and must be called exactly once.
    CHECK(slots.bootRequests==0);
    CHECK(!s.handle(press(true),now));
    CHECK(!s.handle(tap(234,400),now));
    CHECK(!s.handle(home(),now));
    CHECK(s.model().screen==ScreenId::External);
    CHECK(s.model().external.phase==ExternalPhase::BootCommitting);
    // The runtime issues the call only after the committing frame was painted.
    CHECK(s.commitPendingBoot(now));
    CHECK(slots.bootRequests==1 && slots.bootSlot==1);
    m=s.model();
    CHECK(m.external.phase==ExternalPhase::BootFailed);
    CHECK(std::string(m.external.message)=="ESP_ERR_IMAGE_INVALID");
    // Exactly once: a second pass must not retry.
    CHECK(!s.commitPendingBoot(now));
    CHECK(slots.bootRequests==1);
    // Input and home work again, and the failure restarted nothing.
    CHECK(externalButtonCount(m.external)==1);
    CHECK(s.handle(home(),now));
    CHECK(s.model().screen==ScreenId::Home);
    CHECK(slots.bootRequests==1);
}

// The two conditions task 6 left open for task 5 (plan.md 8.2 steps 4 and 5).
void failedBootKeepsMeasuring() {
    FakeSlotService slots;
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    ScreenManager s; TimeUs now=0;
    s.bindSlots(&slots); s.setSlots(slots.catalog);
    openRow(s,now,AppId::Stopwatch);
    s.handle(press(false),now); now+=1000;                 // B starts it
    CHECK(s.stopwatch().state()==StopwatchState::Running);
    s.handle(home(),now); now+=1000;
    openRow(s,now,AppId::External1);
    now+=1000;
    CHECK(s.commitPendingBoot(now));                       // the fake fails
    CHECK(s.model().external.phase==ExternalPhase::BootFailed);
    // A failed launch restarts nothing, so the measurement is untouched and
    // still advancing (plan.md 8.2 step 4).
    CHECK(s.stopwatch().state()==StopwatchState::Running);
    const auto before=s.stopwatch().elapsed(now);
    now+=500000;
    CHECK(s.stopwatch().elapsed(now)>before);
    // Home works again after a failure, and the screen shows it still running.
    CHECK(s.handle(home(),now));
    openRow(s,now,AppId::Stopwatch);
    CHECK(s.model().stopwatch.state==StopwatchState::Running);
}

void successfulBootEndsTheMeasurement() {
    FakeSlotService slots;
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    slots.bootSucceeds=true;
    ScreenManager s; TimeUs now=0;
    s.bindSlots(&slots); s.setSlots(slots.catalog);
    openRow(s,now,AppId::Stopwatch);
    s.handle(press(false),now); now+=1000;
    CHECK(s.stopwatch().state()==StopwatchState::Running);
    s.handle(home(),now); now+=1000;
    openRow(s,now,AppId::External1);
    now+=1000;
    // Returns without a failure: on a device this call ends in a restart, and
    // the shutdown callback has already run on this task (plan.md 8.2 step 5).
    CHECK(!s.commitPendingBoot(now));
    CHECK(slots.bootRequests==1);
    CHECK(s.stopwatch().state()==StopwatchState::Paused);
    const auto frozen=s.stopwatch().elapsed(now);
    now+=500000;
    CHECK(s.stopwatch().elapsed(now)==frozen);
    // The commit is still the one-way stretch it was: nothing takes input.
    CHECK(s.model().external.phase==ExternalPhase::BootCommitting);
    CHECK(!s.handle(home(),now));
}

void homeDuringScanKeepsResultsHarmless() {
    FakeSlotService slots;
    slots.catalog=scanning();
    ScreenManager s; TimeUs now=0;
    s.bindSlots(&slots); s.setSlots(slots.catalog);
    openRow(s,now,AppId::External1);
    CHECK(s.model().screen==ScreenId::External);
    s.handle(home(),now); now+=1000;
    CHECK(s.model().screen==ScreenId::Home);
    // A result that lands after home only updates rows: it must not reopen the
    // detail, move the screen or start a boot (plan.md 8.2).
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    s.setSlots(slots.catalog);
    const auto m=s.model();
    CHECK(m.screen==ScreenId::Home);
    CHECK(std::string(m.names[2])=="KantanPlay");
    CHECK(!m.rowDimmed[2]);
    CHECK(slots.bootRequests==0);
    CHECK(!s.commitPendingBoot(now));
}

void scanFinishingDuringDetailDoesNotLaunch() {
    FakeSlotService slots;
    slots.catalog=scanning();
    ScreenManager s; TimeUs now=0;
    s.bindSlots(&slots); s.setSlots(slots.catalog);
    openRow(s,now,AppId::External1);
    CHECK(s.model().external.status==SlotStatus::Scanning);
    CHECK(s.model().external.phase==ExternalPhase::Browsing);
    // The slot turns launchable underneath the open screen. Only a decision in
    // the list starts a boot, so this must not turn into one.
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    s.setSlots(slots.catalog);
    auto m=s.model();
    CHECK(m.external.status==SlotStatus::Ready);
    CHECK(m.external.phase==ExternalPhase::Browsing);
    CHECK(externalButtonCount(m.external)==1);
    CHECK(slots.bootRequests==0);
    CHECK(!s.commitPendingBoot(now));
    // This is also the only way to read a launchable slot's version, since
    // deciding its row launches it instead of opening this screen.
    CHECK(std::string(m.external.version)=="1.2.0");
    s.handle(press(false),now); now+=1000;
    CHECK(s.model().screen==ScreenId::AppList);
    CHECK(slots.bootRequests==0);
    // Leaving resets the phase, so the next decision starts from scratch.
    openRow(s,now,AppId::External1);
    CHECK(s.model().external.phase==ExternalPhase::BootCommitting);
}

void runtimeScansBehindTheFirstFrameAndPollsResults() {
    StubHal hal; StubRender render; DisplayDataSource data; FakeSlotService slots;
    slots.catalog=scanning(); slots.pending=false;
    AppRuntime runtime(hal,render,data,468,468);
    runtime.bindSlots(slots);
    runtime.begin();
    CHECK(slots.scanRequests==0);
    hal.time=1000; runtime.step();
    // The clock is drawn first, then the second of flash reads is asked for.
    CHECK(render.draws==1);
    CHECK(slots.scanRequests==1);
    hal.time+=20000; runtime.step();
    CHECK(slots.scanRequests==1);               // Requested once, never again.
    // A result reaches the list on the next pass without any extra wakeup.
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    hal.time+=20000; runtime.step();
    CHECK(std::string(render.last.names[2])=="KantanPlay");
    CHECK(!render.last.rowDimmed[2] && render.last.rowDimmed[3]);
}

void runtimeIssuesTheBootAfterPaintingTheCommitFrame() {
    StubHal hal; StubRender render; DisplayDataSource data; FakeSlotService slots;
    slots.set(1,SlotStatus::Ready,"KantanPlay","1.2.0");
    AppRuntime runtime(hal,render,data,468,468);
    runtime.bindSlots(slots);
    runtime.begin();
    hal.time=1000; runtime.step();
    auto pressA=[&]() {
        hal.input.a=true; hal.time+=20000; runtime.step();
        hal.input.a=false; hal.time+=20000; runtime.step();
        hal.time+=200000; runtime.step();
    };
    auto pressB=[&]() {
        hal.input.b=true; hal.time+=20000; runtime.step();
        hal.input.b=false; hal.time+=20000; runtime.step();
        hal.time+=200000; runtime.step();
    };
    pressA();                                   // clock -> list
    while (AppRegistry[runtime.model().list.selection].id!=AppId::External1) pressA();
    const int drawsBefore=render.draws;
    pressB();                                   // decide == launch
    // The committing frame reached the renderer, and only then the API ran.
    CHECK(render.draws>drawsBefore);
    CHECK(slots.bootRequests==1 && slots.bootSlot==1);
    CHECK(runtime.model().external.phase==ExternalPhase::BootFailed);
}
}
int main() {
    listShowsNamesAndDimming();
    unsupportedLayoutDisablesEveryExternalRow();
    launchableRowStartsWithoutConfirmation();
    unusableSlotsOpenTheScreenInstead();
    commitSuppressesInputAndRecovers();
    failedBootKeepsMeasuring();
    successfulBootEndsTheMeasurement();
    homeDuringScanKeepsResultsHarmless();
    scanFinishingDuringDetailDoesNotLaunch();
    runtimeScansBehindTheFirstFrameAndPollsResults();
    runtimeIssuesTheBootAfterPaintingTheCommitFrame();
    std::cout << "multifirm tests passed\n";
    return 0;
}
