#pragma once

#include <cstdint>

namespace app {

enum class Screen : uint8_t { Watch, AppList };

struct State {
    Screen screen = Screen::Watch;
    int selected = 0;
    float listScroll = 0.0f;
    float transition = 0.0f;
    const char* toast = nullptr;
    uint32_t toastUntilMs = 0;
};

class LauncherApp {
public:
    static constexpr int AppCount = 3;

    const State& state() const { return state_; }
    void next(uint32_t nowMs);
    void decide(uint32_t nowMs);
    void home();
    void dragWatch(float dy);
    void dragList(float dy);
    void finishDrag(float totalDy);
    bool tick(uint32_t nowMs);

private:
    State state_{};
};

}  // namespace app
