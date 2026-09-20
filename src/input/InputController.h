#pragma once
#include <cstdint>

namespace launcher {
using TimeUs = int64_t;
struct InputSnapshot { bool a = false, b = false, touching = false; int x = 0, y = 0; };
enum class Gesture { None, Tap, DragStart, DragMove, DragEnd, Cancel };
struct Events {
    bool activity = false, next = false, decide = false, home = false;
    Gesture gesture = Gesture::None;
    int x = 0, y = 0, dx = 0, dy = 0, totalX = 0, totalY = 0;
};
class InputController {
public:
    explicit InputController(int dragThreshold = 9) : threshold_(dragThreshold) {}
    Events update(TimeUs now, const InputSnapshot& in, bool consumeTouch = false);
private:
    InputSnapshot previous_{};
    bool chordGroup_ = false, timing_ = false, homeSent_ = false;
    bool swallowed_ = false, dragging_ = false;
    TimeUs chordSince_ = 0;
    int startX_ = 0, startY_ = 0, threshold_;
};
}
