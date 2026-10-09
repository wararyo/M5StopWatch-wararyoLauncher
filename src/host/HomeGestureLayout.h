#pragma once
#include "core/Time.h"
#include "ui/rendering/Scale.h"
#include <algorithm>
#include <cmath>
#include <optional>
namespace launcher {
// The home gesture's geometry and timing (docs/task14/plan.md 2): where a
// swipe down from the top edge starts, how tall its band grows, where it is let
// go to go home, and the easings of the band and of the clock coming in. Pure,
// so the host tests check them without a display.
//
// Lengths are those of the 466px panel, scaled like every other layout.
inline constexpr TimeUs HomeGestureFrameUs=16000;
inline constexpr TimeUs HomeBandShrinkUs=100000;
inline constexpr TimeUs HomeRevealUs=600000;
// A touch that starts this close to the top belongs to the system.
inline int homeGestureEdge(Viewport v) { return scaled(v,40); }
inline bool homeGestureStarts(Viewport v,int y) { return y<=homeGestureEdge(v); }
// The band never grows past this.
inline int homeBandMax(Viewport v) { return scaled(v,80); }
// Let go this far down, the swipe goes home. Only y counts.
inline int homeGestureCommitY(Viewport v) { return scaled(v,180); }
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
// Out, a pause two thirds of the way there half way through, then in.
inline float easeOutInCubic(float t) {
    constexpr float rest=2.0f/3;
    t=std::clamp(t,0.0f,1.0f);
    const float u=2*t-1,c=u*u*u;
    return u<0 ? rest*(1+c) : rest+(1-rest)*c;
}
// The band while A and B are held: out to a swipe's maximum, a pause there,
// then on with gathering speed into the clock coming in as home fires.
inline float homeChordBand(Viewport v,TimeUs held,TimeUs homeHoldUs) {
    return scaled(v,120)*easeOutInCubic(float(held)/float(homeHoldUs));
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
// What a frame shows of the gesture: the band's height, 0 when there is none,
// and, once home was reached, the clock coming in from the top down to `edge`
// over what the screen left on the panel (docs/task14/plan.md 2.5).
struct HomeGestureModel {
    int band=0;
    bool revealing=false;
    int edge=0;
    bool shown() const { return band>0; }
};
// How one frame of the clock coming in is drawn (docs/task14/plan.md 2.6).
// Its first frame is the change of screen, which repaints in full as any does,
// only inside `within`: that turns the band into the clock.
struct HomeRevealFrame {
    // Where the frame may draw (Renderer::draw): from the top down to the
    // edge. None when the clock is not coming in.
    std::optional<Rect> within;
    // The rows the edge moved over since the last frame: new to the clock,
    // though none of its elements changed. On the frame after the last one it
    // is the rest of the panel, drawn with no range.
    Rect strip{};
};
// Follows the edge from frame to frame, so each frame draws only the rows it
// uncovered: the screen under the edge is never drawn again, and the whole
// way home paints about one panel's worth.
class HomeRevealTracker {
public:
    HomeRevealFrame next(const HomeGestureModel& g,Viewport v) {
        HomeRevealFrame f;
        if (g.revealing) {
            const int edge=std::clamp(g.edge,0,v.height);
            if (shown_<0) shown_=edge;
            else if (edge>shown_) { f.strip={0,shown_,v.width,edge-shown_}; shown_=edge; }
            f.within=Rect{0,0,v.width,shown_};
        } else if (shown_>=0) {
            if (shown_<v.height) f.strip={0,shown_,v.width,v.height-shown_};
            shown_=-1;
        }
        return f;
    }
private:
    int shown_=-1; // The rows painted so far, -1 while the clock is not coming in.
};
}
