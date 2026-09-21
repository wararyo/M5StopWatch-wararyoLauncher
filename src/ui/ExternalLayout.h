#pragma once
#include "ListLayout.h"
namespace launcher {
// Three lines of detail: slot number, state or version, diagnostic.
constexpr int ExternalLines=3;
// Ready offers a launch and a cancel; everything else only goes back, and the
// committing frame offers nothing at all (plan.md 8.2).
inline int externalButtonCount(const ExternalModel& e) {
    if (e.phase==ExternalPhase::BootCommitting) return 0;
    if (e.phase==ExternalPhase::BootFailed) return 1;
    return e.status==SlotStatus::Ready ? 2 : 1;
}
inline Rect externalTitleBox(const ScreenModel& m) {
    const int h=offsetPx(m,52),margin=offsetPx(m,58);
    return {margin,offsetPx(m,118)-h/2,m.width-2*margin,h};
}
inline Rect externalLineBox(const ScreenModel& m,int line) {
    const int h=offsetPx(m,46),margin=offsetPx(m,46);
    return {margin,offsetPx(m,204)+line*offsetPx(m,54)-h/2,m.width-2*margin,h};
}
inline Rect externalButtonBox(const ScreenModel& m,int index) {
    const int count=externalButtonCount(m.external);
    const int w=offsetPx(m,count==1 ? 150 : 138),h=offsetPx(m,52);
    const int cx=count==1 ? m.width/2 : m.width/2+offsetPx(m,index==0 ? -74 : 74);
    return {cx-w/2,offsetPx(m,400)-h/2,w,h};
}
// Same rule as the settings screen: nothing outside a painted target can be
// tapped, because drawing and hit testing read the same rectangles.
struct ExternalHit { enum Kind { None,Button } kind=None; int index=0; };
inline ExternalHit hitExternal(const ScreenModel& m,int x,int y) {
    for (int i=0;i<externalButtonCount(m.external);++i)
        if (externalButtonBox(m,i).contains(x,y)) return {ExternalHit::Button,i};
    return {};
}
}
