#pragma once

#include <cstdint>
#include <M5Unified.h>

namespace input {

struct Events {
    bool activity = false;
    bool next = false;
    bool decide = false;
    bool home = false;
    bool touchStarted = false;
    bool touchMoved = false;
    bool touchEnded = false;
    float dragDeltaY = 0.0f;
    float dragTotalY = 0.0f;
};

class InputController {
public:
    Events update(uint32_t nowMs, const m5::touch_detail_t& touch, bool swallowTouch);

private:
    bool aSeen_ = false;
    bool bSeen_ = false;
    bool chord_ = false;
    bool homeSent_ = false;
    uint32_t chordSinceMs_ = 0;
    bool dragging_ = false;
    bool swallowGesture_ = false;
    int startY_ = 0;
    int previousY_ = 0;
};

}  // namespace input

