#include "HomeGesture.h"
namespace launcher {
void HomeGesture::reset() {
    phase_=Phase::Idle; band_=0; frame_=INT64_MAX;
    touch_=follow_=false;
}
void HomeGesture::shrink(TimeUs now) {
    if (band_<=0) { reset(); return; }
    phase_=Phase::Shrink; shrinkFrom_=band_; shrinkStart_=now;
    frame_=now+HomeGestureFrameUs;
}
HomeGestureOutcome HomeGesture::handle(const Events& e,TimeUs now,bool swipe,bool chord) {
    HomeGestureOutcome out;
    if (e.chordChanged) {
        if (e.chord && chord) {
            // The hold takes the band over, from a swipe too: that touch is
            // still the system's, but no longer moves anything.
            phase_=Phase::Chord; chordSince_=e.chordSince; follow_=false;
            band_=homeChordBand(viewport_,now-chordSince_,InputController::HomeHoldUs);
            frame_=now+HomeGestureFrameUs;
            out.changed=true;
        } else if (!e.chord && phase_==Phase::Chord) {
            // Let go short of home, or spent.
            shrink(now); out.changed=true;
        }
    }
    // Decided once, where the finger lands: the whole touch then belongs to
    // the system, and no screen sees any of it, the start included.
    if (e.gesture==Gesture::TouchStart) {
        touch_=swipe && homeGestureStarts(viewport_,e.y);
        follow_=touch_ && phase_!=Phase::Chord;
    }
    if (!touch_ || e.gesture==Gesture::None) return out;
    out.consumed=true;
    switch (e.gesture) {
    case Gesture::DragStart: case Gesture::DragMove:
        if (!follow_) break;
        // A band still shrinking from the last swipe is taken up by this one.
        phase_=Phase::Swipe; frame_=INT64_MAX;
        band_=homeBandHeight(viewport_,e.y);
        out.changed=true;
        break;
    case Gesture::DragEnd: case Gesture::Tap: case Gesture::Cancel:
        touch_=false;
        if (!follow_ || phase_!=Phase::Swipe) break;
        follow_=false;
        if (e.gesture==Gesture::DragEnd && homeGestureCommits(viewport_,e.y)) out.home=true;
        else shrink(now);
        out.changed=true;
        break;
    default: break;
    }
    return out;
}
bool HomeGesture::update(TimeUs now) {
    if (now<frame_) return false;
    if (phase_==Phase::Chord) {
        const TimeUs held=now-chordSince_;
        band_=homeChordBand(viewport_,held,InputController::HomeHoldUs);
        // Full when home fires; nothing more to draw until it does.
        frame_=held<InputController::HomeHoldUs ? now+HomeGestureFrameUs : INT64_MAX;
        return true;
    }
    if (phase_==Phase::Shrink) {
        const TimeUs elapsed=now-shrinkStart_;
        if (elapsed>=HomeBandShrinkUs) { band_=0; phase_=Phase::Idle; frame_=INT64_MAX; }
        else { band_=homeBandShrinking(shrinkFrom_,elapsed); frame_=now+HomeGestureFrameUs; }
        return true;
    }
    frame_=INT64_MAX;
    return false;
}
}
