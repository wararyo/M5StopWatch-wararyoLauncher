#pragma once
#include "ui/list/ListLayout.h"
namespace launcher {
// The list's incoming edge: at `transition` 0 it sits a full panel below, at
// 1 it is at the top. Its opaque background, its rows and the clock's
// uncovered clip all follow this one line (docs/task10/plan-10-4.md 4).
inline int appListTop(Viewport v,float transition) {
    return static_cast<int>((1-transition)*v.height);
}
// What the list covers: from its edge down, the full width.
inline Rect appListCover(Viewport v,float transition) {
    const int top=appListTop(v,transition);
    return {0,top,v.width,std::max(0,v.height-top)};
}
// The rest of the panel, where the clock still shows.
inline Rect appListUncovered(Viewport v,float transition) {
    return {0,0,v.width,std::clamp(appListTop(v,transition),0,v.height)};
}
// The pixels the list's background changes from one frame to the next: all
// of both areas for another colour, else the band between the two edges
// (both areas reach the bottom at full width), or nothing.
inline Rect appListCoverChange(Rect before,uint16_t beforeColor,Rect after,uint16_t afterColor) {
    if (beforeColor!=afterColor || before.empty() || after.empty()) {
        if (beforeColor==afterColor && before==after) return {};
        return unite(before,after);
    }
    if (before==after) return {};
    const int top=std::min(before.y,after.y),bottom=std::max(before.y,after.y);
    return {after.x,top,after.w,bottom-top};
}
// The list slides up over the clock. Its incoming edge clips it, so no row is
// drawn below the part it covers. Input and drawing both place the list
// through here.
inline ListPlacement appListPlacement(Viewport v,float transition,float scroll) {
    const int offset=appListTop(v,transition);
    return {{v,offset,appListCover(v,transition)},scroll};
}
}
