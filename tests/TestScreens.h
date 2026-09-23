#pragma once
#include "app/ScreenManager.h"
namespace launcher {
// What a ScreenManager borrows from the application, owned next to it the way
// app/Application.h owns it. A base, so it is built before the manager.
struct AppState {
    StopwatchService stopwatch;
    RuntimeSettings runtime;
};
// A ScreenManager for tests that drive it on its own, without a runtime.
struct TestScreens : AppState, ScreenManager {
    explicit TestScreens(int width=468,int height=468) : ScreenManager(stopwatch,runtime,width,height) {}
};
}
