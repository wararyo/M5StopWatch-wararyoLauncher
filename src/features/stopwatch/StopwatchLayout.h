#pragma once
#include "ui/rendering/Scale.h"
#include "features/stopwatch/StopwatchModel.h"
namespace launcher {
// UserDemo's stopwatch, in its own 468 basis: two round buttons over a rounded
// panel that carries the elapsed time and the lap list. Drawing and hit testing
// share these rectangles, so nothing outside a painted target can be tapped.
//
// Every box here is FIXED for the life of the screen. The panel is background
// rather than a plan element (see StopwatchLayer) and is repainted wherever
// the frame restores, so the boxes only have to cover what their text draws.
inline Rect stopwatchButtonBox(const Viewport& m,int index) {
    const int w=offsetPx(m,105),h=offsetPx(m,72);
    const int cx=m.width/2+offsetPx(m,index==0 ? -72 : 72),cy=offsetPx(m,91);
    return {cx-w/2,cy-h/2,w,h};
}
inline int stopwatchButtonRadius(const Viewport& m) { return offsetPx(m,34); }
inline Rect stopwatchPanelBox(const Viewport& m) {
    const int w=offsetPx(m,466);
    return {m.width/2-w/2,offsetPx(m,148),w,offsetPx(m,330)};
}
inline int stopwatchPanelRadius(const Viewport& m) { return offsetPx(m,60); }
inline Rect stopwatchDividerBox(const Viewport& m) {
    const int w=offsetPx(m,160),h=scaled(m,4);
    return {m.width/2-w/2,offsetPx(m,248)-h/2,w,h};
}
// The clock and the hundredths are two elements side by side, centred together
// on the panel: only the hundredths change at 40 Hz, so only that small box is
// repainted. Both are sized from the widest value they can ever hold.
inline Rect stopwatchClockBox(const Viewport& m) {
    return {m.width/2-offsetPx(m,174),offsetPx(m,198)-offsetPx(m,34),
            offsetPx(m,276),offsetPx(m,68)};
}
inline Rect stopwatchFractionBox(const Viewport& m) {
    return {m.width/2+offsetPx(m,102),offsetPx(m,198)-offsetPx(m,34),
            offsetPx(m,72),offsetPx(m,68)};
}
// Three rows. UserDemo lays out a fourth, but at y=434 the circle leaves only
// +-142px of chord and the "LAP n" label falls off it, so the list stops at
// what the safe area holds (plan.md 2.1).
inline Rect stopwatchLapBox(const Viewport& m,int row) {
    const int h=offsetPx(m,44),left=offsetPx(m,56);
    return {left,offsetPx(m,290+48*row)-h/2,m.width-2*left,h};
}
inline int stopwatchLapPadding(const Viewport& m) { return offsetPx(m,8); }
struct StopwatchHit { enum Kind { None,Left,Right } kind=None; };
inline StopwatchHit hitStopwatch(const Viewport& m,int x,int y) {
    if (stopwatchButtonBox(m,0).contains(x,y)) return {StopwatchHit::Left};
    if (stopwatchButtonBox(m,1).contains(x,y)) return {StopwatchHit::Right};
    return {};
}
// Button labels. The left button is dead in Reset, which the greyed label says
// (plan.md 5.3); the right one names what it will do next.
inline const char* stopwatchLeftLabel(StopwatchState state) {
    return state==StopwatchState::Paused ? "RESET" : "LAP";
}
inline const char* stopwatchRightLabel(StopwatchState state) {
    return state==StopwatchState::Running ? "STOP" : "START";
}
}
