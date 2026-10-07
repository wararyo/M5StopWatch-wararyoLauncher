#pragma once
#include "ui/rendering/Scale.h"
#include "ui/rendering/Geometry.h"
namespace launcher {
// The pedometer screen, laid out from docs/Images/Pedometer/Steps.svg (466
// across). Like the timer's, coordinates are taken about the centre of that
// drawing and scaled, so the circle's margins hold on the 468 panel. Drawing
// and hit testing share these boxes.
inline int pedometerX(const Viewport& m,int x) { return m.width/2+offsetPx(m,x-233); }
inline int pedometerY(const Viewport& m,int y) { return m.height/2+offsetPx(m,y-233); }
inline Rect pedometerBox(const Viewport& m,int x,int y,int w,int h) {
    const int left=pedometerX(m,x),top=pedometerY(m,y);
    return {left,top,pedometerX(m,x+w)-left,pedometerY(m,y+h)-top};
}
inline Rect pedometerIconBox(const Viewport& m) { return pedometerBox(m,211,17,44,44); }
// "今日の歩数", whose glyphs span y 145-171.
inline Rect pedometerTitleBox(const Viewport& m) { return pedometerBox(m,63,138,340,40); }
// The count and its unit, centred together: wide enough for seven figures
// and the English unit. Both sit on the figures' baseline.
inline Rect pedometerCountBox(const Viewport& m) { return pedometerBox(m,33,210,400,72); }
inline int pedometerBaseline(const Viewport& m) { return pedometerY(m,266); }
// Between the figures and the unit.
inline int pedometerUnitGap(const Viewport& m) { return offsetPx(m,10); }
inline Rect pedometerOkBox(const Viewport& m) { return pedometerBox(m,153,360,160,44); }
// OK is low and thin above the bezel, so like the timer's SET it takes touches
// from as far again below it as it is tall; what it shows stays the same.
inline Rect pedometerOkHitBox(const Viewport& m) { return pedometerBox(m,153,360,160,88); }
inline int pedometerCorner(const Viewport& m) { return offsetPx(m,8); }
}
