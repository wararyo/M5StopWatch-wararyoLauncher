#include "InputController.h"
#include <cstdlib>
namespace launcher {
Events InputController::update(TimeUs now, const InputSnapshot& in, bool consumeTouch) {
    Events out{};
    out.activity = in.a || in.b || in.touching || previous_.a || previous_.b || previous_.touching;
    if (in.a && in.b) {
        chordGroup_ = true;
        if (!timing_) { timing_ = true; chordSince_ = now; }
        if (!homeSent_ && now - chordSince_ >= 600000) out.home = homeSent_ = true;
    } else timing_ = false;
    if (!chordGroup_) {
        out.next = previous_.a && !in.a;
        out.decide = previous_.b && !in.b;
    }
    if (!in.a && !in.b) chordGroup_ = homeSent_ = false;

    out.x = in.x; out.y = in.y;
    if (in.touching && !previous_.touching) {
        startX_ = in.x; startY_ = in.y;
        dragging_ = false; swallowed_ = consumeTouch;
    }
    if (consumeTouch && in.touching) swallowed_ = true;
    // Home cancels the whole gesture, including a release in this same sample.
    if (out.home) {
        out.next = out.decide = false;
        out.gesture = Gesture::Cancel;
        swallowed_ = in.touching;
        dragging_ = false;
    } else if (in.touching && !swallowed_) {
        out.totalX = in.x - startX_; out.totalY = in.y - startY_;
        out.dx = in.x - previous_.x; out.dy = in.y - previous_.y;
        if (!dragging_ && (std::abs(out.totalX) > threshold_ || std::abs(out.totalY) > threshold_)) {
            dragging_ = true; out.gesture = Gesture::DragStart;
        } else if (dragging_ && (out.dx || out.dy)) out.gesture = Gesture::DragMove;
    } else if (!in.touching && previous_.touching && !swallowed_) {
        out.x = previous_.x; out.y = previous_.y;
        out.totalX = previous_.x - startX_; out.totalY = previous_.y - startY_;
        out.gesture = dragging_ ? Gesture::DragEnd : Gesture::Tap;
    }
    if (!in.touching) swallowed_ = dragging_ = false;
    previous_ = in;
    return out;
}
}
