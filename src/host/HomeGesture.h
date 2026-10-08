#pragma once
#include "host/HomeGestureLayout.h"
#include "input/InputController.h"
namespace launcher {
// What one input did to the home gesture.
struct HomeGestureOutcome {
    bool changed=false;  // The band moved.
    bool home=false;     // The swipe was let go far enough down: go home.
    bool consumed=false; // The touch is the system's: no screen gets it.
};
// The system's way home besides the A+B hold (docs/task14/plan.md): a swipe
// down from the top edge of an app screen, and the band that shows how far it
// (or the A+B hold) has got. ScreenManager owns it and asks it before any
// screen, as it does home. It never changes screens itself: it says when the
// swipe goes home, and the manager leaves everything as home does.
class HomeGesture {
public:
    explicit HomeGesture(Viewport viewport={}):viewport_(viewport) {}
    // `swipe`: an app screen is open, so a touch at the top edge is the
    // system's. `chord`: home would close something (an app screen or the
    // list), so the A+B hold shows its band.
    HomeGestureOutcome handle(const Events& e,TimeUs now,bool swipe,bool chord);
    // Advances the A+B band, the shrinking band and the clock coming in; true
    // when any of them moved.
    bool update(TimeUs now);
    TimeUs nextUpdate() const { return frame_; }
    bool active() const { return phase_!=Phase::Idle; }
    // Something replaced the screen under the gesture (an attention request):
    // the band or the clock coming in goes at once, and so does a touch the
    // system was following, whose end the input no longer reports.
    void reset();
    // The band's height now, which is where the clock starts coming in.
    float band() const { return band_; }
    // Home was reached over a screen or the list: the band gives way to the
    // clock, which comes in from where the band was down to the bottom.
    void reveal(float from,TimeUs now);
    bool revealing() const { return phase_==Phase::Reveal; }
    HomeGestureModel model() const {
        HomeGestureModel m;
        m.band=phase_==Phase::Reveal ? 0 : int(std::lround(band_));
        m.revealing=phase_==Phase::Reveal; m.edge=edge_;
        return m;
    }
private:
    enum class Phase { Idle,Swipe,Chord,Shrink,Reveal };
    void shrink(TimeUs now);
    Viewport viewport_{};
    Phase phase_=Phase::Idle;
    // The current touch started at the top edge of an app screen: the system
    // keeps it until the finger lifts, whatever the band does meanwhile. It
    // moves the band (`follow_`) until the A+B hold takes the band over.
    bool touch_=false,follow_=false;
    float band_=0,shrinkFrom_=0,revealFrom_=0;
    int edge_=0;
    TimeUs chordSince_=0,shrinkStart_=0,revealStart_=0,frame_=INT64_MAX;
};
}
