#pragma once
#include "host/ScreenManager.h"
#include "features/home/faces/DigitalLayout.h"
namespace launcher {
// The clock's input as Digital answers it, without its drawing.
struct TestHome : HomeControlPort {
    DigitalControl digital;
    Viewport viewport{};
    int events=0;
    HomeEvent last{};
    HomeOutcome handle(const HomeEvent& e) override { ++events; last=e; return digital.handle(e,viewport); }
};
// What a ScreenManager borrows from the application, owned next to it the way
// host/HostApplication.h owns it. A base, so it is built before the manager.
struct AppState {
    StopwatchService stopwatch;
    RuntimeSettings runtime;
    TestHome home;
};
// A ScreenManager for tests that drive it on its own, without a runtime.
struct TestScreens : AppState, ScreenManager {
    explicit TestScreens(int width=468,int height=468) : ScreenManager(stopwatch,runtime,width,height) {
        home.viewport={width,height}; bindHome(&home);
    }
};
}
