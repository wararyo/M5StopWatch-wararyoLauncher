#pragma once
#include "core/Time.h"
#include <cstdint>

namespace launcher {
struct InputSnapshot { bool a = false, b = false, touching = false; int x = 0, y = 0; };
enum class Gesture { None, TouchStart, Tap, DragStart, DragMove, DragEnd, Cancel, LongPress };
// The button held down on its own, if any.
enum class Hold : uint8_t { None, A, B };
struct Events {
    bool activity = false, next = false, decide = false, home = false;
    Gesture gesture = Gesture::None;
    int x = 0, y = 0, dx = 0, dy = 0, totalX = 0, totalY = 0;
    float velocityY = 0; // Screen coordinates, pixels per second at release.
    // How long the released button was down, with `next` or `decide`, and how
    // long the finger was, with a tap. Whether that makes a long press is the
    // screen's to decide (docs/task12/plan.md 2.3); input only reports it.
    TimeUs pressUs = 0, touchUs = 0;
    // The button held on its own right now, and since when. A chord, the
    // release or discardHeld() ends it, and `holdChanged` marks the sample in
    // which it starts or ends, so a screen can follow a hold with its own
    // deadlines instead of being sampled every input period.
    Hold hold = Hold::None;
    TimeUs holdSince = 0;
    bool holdChanged = false;
    // A and B held together on their way to home, and since when: from the
    // moment both are down until home fires, either is let go or the pair is
    // spent. `chordChanged` marks the sample in which that starts or ends, so
    // the system can show how far the hold has got (docs/task14/plan.md 2.4).
    bool chord = false, chordChanged = false;
    TimeUs chordSince = 0;
};
class InputController {
public:
    // A touch held still this long on the resting clock (docs/task10/plan.md 3.1).
    static constexpr TimeUs LongPressUs = 600000;
    // A and B held together this long are home (docs/plan.md 5.1).
    static constexpr TimeUs HomeHoldUs = 600000;
    explicit InputController(int dragThreshold = 9) : threshold_(dragThreshold) {}
    // `longPress` says whether the clock is at rest and takes a long press now.
    // Only a touch that starts while it does can become one, so the other
    // screens keep reading a long press as a tap, and a touch that began
    // elsewhere does not turn into one when home arrives under it. Such a
    // touch that is not a drag is consumed once the clock stops being at rest
    // (the list opened by a button, say): it ends without a tap.
    Events update(TimeUs now, const InputSnapshot& in, bool consumeTouch = false, bool longPress = false);
    // Everything held as of the last sample is spent until it is let go: no
    // release, hold, home or gesture comes of it, so a screen that replaced
    // the one it was meant for never receives it (docs/task12/plan.md 2.4).
    // A press that starts after this is new. Called before the next update().
    void discardHeld();
private:
    InputSnapshot previous_{};
    bool chordGroup_ = false, timing_ = false, homeSent_ = false;
    bool spent_ = false;
    bool swallowed_ = false, dragging_ = false;
    bool homeTouch_ = false, longPressArmed_ = false, chord_ = false;
    Hold hold_ = Hold::None;
    TimeUs chordSince_ = 0, touchSince_ = 0, pressA_ = 0, pressB_ = 0;
    TimeUs sampleTime_ = 0, lastMoveTime_ = 0;
    float velocityY_ = 0;
    int startX_ = 0, startY_ = 0, threshold_;
};
}
