#pragma once
#include "core/Time.h"
#include <cstdint>

namespace launcher {
struct InputSnapshot { bool a = false, b = false, touching = false; int x = 0, y = 0; };
enum class Gesture { None, TouchStart, Tap, DragStart, DragMove, DragEnd, Cancel, LongPress };
struct Events {
    bool activity = false, next = false, decide = false, home = false;
    Gesture gesture = Gesture::None;
    int x = 0, y = 0, dx = 0, dy = 0, totalX = 0, totalY = 0;
    float velocityY = 0; // Screen coordinates, pixels per second at release.
};
class InputController {
public:
    // A touch held still this long on the resting clock (docs/task10/plan.md 3.1).
    static constexpr TimeUs LongPressUs = 600000;
    explicit InputController(int dragThreshold = 9) : threshold_(dragThreshold) {}
    // `longPress` says whether the clock is at rest and takes a long press now.
    // Only a touch that starts while it does can become one, so the other
    // screens keep reading a long press as a tap, and a touch that began
    // elsewhere does not turn into one when home arrives under it. Such a
    // touch that is not a drag is consumed once the clock stops being at rest
    // (the list opened by a button, say): it ends without a tap.
    Events update(TimeUs now, const InputSnapshot& in, bool consumeTouch = false, bool longPress = false);
private:
    InputSnapshot previous_{};
    bool chordGroup_ = false, timing_ = false, homeSent_ = false;
    bool swallowed_ = false, dragging_ = false;
    bool homeTouch_ = false, longPressArmed_ = false;
    TimeUs chordSince_ = 0, touchSince_ = 0;
    TimeUs sampleTime_ = 0, lastMoveTime_ = 0;
    float velocityY_ = 0;
    int startX_ = 0, startY_ = 0, threshold_;
};
}
