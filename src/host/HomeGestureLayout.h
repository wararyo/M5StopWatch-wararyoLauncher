#pragma once
#include "core/Time.h"
#include "ui/rendering/Scale.h"
#include <algorithm>
#include <cmath>
namespace launcher {
// The home gesture's geometry and timing (docs/task14/plan.md 2): where a
// swipe down from the top edge starts, how tall its band grows, where it is let
// go to go home, and the easings of the band and of the clock coming in. Pure,
// so the host tests check them without a display.
//
// Lengths are those of the 466px panel, scaled like every other layout.
inline constexpr TimeUs HomeGestureFrameUs=16000;
inline constexpr TimeUs HomeBandShrinkUs=100000;
inline constexpr TimeUs HomeRevealUs=800000;
// A touch that starts this close to the top belongs to the system.
inline int homeGestureEdge(Viewport v) { return scaled(v,40); }
inline bool homeGestureStarts(Viewport v,int y) { return y<=homeGestureEdge(v); }
// The band never grows past this.
inline int homeBandMax(Viewport v) { return scaled(v,80); }
// Let go this far down, the swipe goes home. Only y counts.
inline int homeGestureCommitY(Viewport v) { return scaled(v,120); }
inline bool homeGestureCommits(Viewport v,int y) { return y>=homeGestureCommitY(v); }
// The band under a finger at `y`: its edge follows the finger near the top (a
// slope of 1) and slows down further down, never reaching the maximum.
inline float homeBandHeight(Viewport v,int y) {
    const float max=float(homeBandMax(v));
    return max*(1-std::exp(-float(std::max(0,y))/max));
}
inline float easeOutQuint(float t) {
    const float u=1-std::clamp(t,0.0f,1.0f);
    return 1-u*u*u*u*u;
}
inline float easeInOutCubic(float t) {
    t=std::clamp(t,0.0f,1.0f);
    if (t<0.5f) return 4*t*t*t;
    const float u=-2*t+2;
    return 1-u*u*u/2;
}
// The band while A and B are held: it reaches the maximum as home fires.
inline float homeChordBand(Viewport v,TimeUs held,TimeUs homeHoldUs) {
    return homeBandMax(v)*easeInOutCubic(float(held)/float(homeHoldUs));
}
// A band let go short of home, `elapsed` after it started to shrink.
inline float homeBandShrinking(float from,TimeUs elapsed) {
    return from*(1-easeOutQuint(float(elapsed)/float(HomeBandShrinkUs)));
}
// The edge of the clock coming in from the top, from where the band was to the
// bottom of the panel.
inline int homeRevealEdge(Viewport v,float from,TimeUs elapsed) {
    const float t=easeOutQuint(float(elapsed)/float(HomeRevealUs));
    return std::clamp(int(std::lround(from+(v.height-from)*t)),0,v.height);
}
// What a frame shows of the gesture: the band's height, 0 when there is none.
struct HomeGestureModel {
    int band=0;
    bool shown() const { return band>0; }
};
}
