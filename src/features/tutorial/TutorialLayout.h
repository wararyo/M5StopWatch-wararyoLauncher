#pragma once
#include "ui/rendering/Scale.h"
#include "ui/rendering/Geometry.h"
namespace launcher {
// The way home (docs/task15/plan.md 2.6): a mark at the top edge where the
// swipe starts, a title and three lines, centred. Nothing here is tapped, so
// there is no hit testing; the mark lies inside the top edge (y <= 40) that
// the home gesture owns, which is allowed for drawing.
inline constexpr int TutorialLines=3;
// The downward chevron, centred in the top edge.
inline Rect tutorialMarkBox(const Viewport& m) {
    const int w=offsetPx(m,44),h=offsetPx(m,24);
    return {m.width/2-w/2,offsetPx(m,10),w,h};
}
inline Rect tutorialTitleBox(const Viewport& m) {
    const int h=offsetPx(m,48);
    return {m.width/4,offsetPx(m,150)-h/2,m.width/2,h};
}
// The lines are 44 apart about the middle of the panel, wide enough for the
// longest of either language with the circle's margin to spare.
inline Rect tutorialLineBox(const Viewport& m,int line) {
    const int h=offsetPx(m,40),margin=offsetPx(m,40);
    return {margin,offsetPx(m,222+line*44)-h/2,m.width-2*margin,h};
}
}
