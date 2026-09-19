#include "LauncherApp.h"
#include <algorithm>

namespace app {

void LauncherApp::next(uint32_t) {
    if (state_.screen == Screen::Watch) {
        state_.screen = Screen::AppList;
        state_.transition = 1.0f;
        return;
    }
    state_.selected = (state_.selected + 1) % AppCount;
}

void LauncherApp::decide(uint32_t nowMs) {
    if (state_.screen == Screen::Watch) {
        state_.screen = Screen::AppList;
        state_.transition = 1.0f;
        return;
    }
    static constexpr const char* messages[] = {
        "STOPWATCH: prototype", "SETTINGS: prototype", "EXTERNAL 1: prototype"};
    state_.toast = messages[state_.selected];
    state_.toastUntilMs = nowMs + 1400;
}

void LauncherApp::home() {
    state_ = {};
}

void LauncherApp::dragWatch(float dy) {
    if (dy >= 0.0f) return;
    state_.transition = std::clamp(-dy / 170.0f, 0.0f, 1.0f);
}

void LauncherApp::dragList(float dy) {
    state_.listScroll = std::clamp(state_.listScroll - dy, 0.0f,
                                   static_cast<float>((AppCount - 1) * 94));
    state_.selected = std::clamp(static_cast<int>((state_.listScroll + 47) / 94),
                                 0, AppCount - 1);
}

void LauncherApp::finishDrag(float totalDy) {
    if (state_.screen == Screen::Watch) {
        if (totalDy < -50.0f) {
            state_.screen = Screen::AppList;
            state_.transition = 1.0f;
        } else {
            state_.transition = 0.0f;
        }
    }
}

bool LauncherApp::tick(uint32_t nowMs) {
    if (state_.toast && static_cast<int32_t>(nowMs - state_.toastUntilMs) >= 0) {
        state_.toast = nullptr;
        return true;
    }
    return false;
}

}  // namespace app
