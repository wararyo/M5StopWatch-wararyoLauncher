#include "InputController.h"

namespace input {
namespace {
constexpr uint32_t HomeHoldMs = 600;
}

Events InputController::update(uint32_t nowMs, const m5::touch_detail_t& touch,
                               bool swallowTouch) {
    Events out{};
    const bool a = M5.BtnA.isPressed();
    const bool b = M5.BtnB.isPressed();
    const bool touching = touch.isPressed();
    out.activity = a || b || touching;

    if (a) aSeen_ = true;
    if (b) bSeen_ = true;
    if (a && b) {
        if (!chord_) {
            chord_ = true;
            chordSinceMs_ = nowMs;
        } else if (!homeSent_ && nowMs - chordSinceMs_ >= HomeHoldMs) {
            out.home = homeSent_ = true;
        }
    }
    if (!a && !b) {
        if (!chord_) {
            out.next = aSeen_;
            out.decide = !aSeen_ && bSeen_;
        }
        aSeen_ = bSeen_ = chord_ = homeSent_ = false;
    }

    if (touch.wasPressed()) {
        dragging_ = true;
        swallowGesture_ = swallowTouch;
        startY_ = previousY_ = touch.y;
        out.touchStarted = !swallowGesture_;
    } else if (touching && dragging_) {
        if (!swallowGesture_) {
            out.touchMoved = true;
            out.dragDeltaY = touch.y - previousY_;
            out.dragTotalY = touch.y - startY_;
        }
        previousY_ = touch.y;
    }
    if (touch.wasReleased() && dragging_) {
        out.touchEnded = !swallowGesture_;
        out.dragTotalY = previousY_ - startY_;
        dragging_ = false;
        swallowGesture_ = false;
    }
    return out;
}

}  // namespace input

