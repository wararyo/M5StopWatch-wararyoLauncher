#pragma once
#include "ui/Scale.h"
#include "features/external/ExternalModel.h"
namespace launcher {
// Three lines of detail: slot number, state or version, diagnostic.
constexpr int ExternalLines=3;
// There is no confirmation to give: a launchable slot starts from the list, so
// this screen only ever goes back. The committing frame takes nothing at all
// (plan.md 8.2).
inline int externalButtonCount(const ExternalModel& e) {
    return e.phase==ExternalPhase::BootCommitting ? 0 : 1;
}
inline Rect externalTitleBox(const Viewport& m) {
    const int h=offsetPx(m,52),margin=offsetPx(m,58);
    return {margin,offsetPx(m,118)-h/2,m.width-2*margin,h};
}
inline Rect externalLineBox(const Viewport& m,int line) {
    const int h=offsetPx(m,46),margin=offsetPx(m,46);
    return {margin,offsetPx(m,204)+line*offsetPx(m,54)-h/2,m.width-2*margin,h};
}
inline Rect externalButtonBox(const Viewport& m,int) {
    const int w=offsetPx(m,150),h=offsetPx(m,52);
    return {m.width/2-w/2,offsetPx(m,400)-h/2,w,h};
}
// Same rule as the settings screen: nothing outside a painted target can be
// tapped, because drawing and hit testing read the same rectangles.
struct ExternalHit { enum Kind { None,Button } kind=None; int index=0; };
inline ExternalHit hitExternal(const Viewport& m,const ExternalModel& external,int x,int y) {
    for (int i=0;i<externalButtonCount(external);++i)
        if (externalButtonBox(m,i).contains(x,y)) return {ExternalHit::Button,i};
    return {};
}
}
