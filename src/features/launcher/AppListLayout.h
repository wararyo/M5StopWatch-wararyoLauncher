#pragma once
#include "ui/list/ListLayout.h"
namespace launcher {
// The list slides up over the clock: at `transition` 0 it sits a full panel
// below, at 1 it is home. Its incoming edge clips it, so no row is drawn over
// the leaving watch face. Input and drawing both place the list through here.
inline ListPlacement appListPlacement(Viewport v,float transition,float scroll) {
    const int offset=static_cast<int>((1-transition)*v.height);
    return {{v,offset,{0,offset,v.width,std::max(0,v.height-offset)}},scroll};
}
// The clock slides up as the list comes in: at `transition` 1 it has left the
// panel. Its clip is the part still on screen, so it ends where the list's
// incoming edge begins.
inline DrawRegion launcherHomeRegion(Viewport v,float transition) {
    const int offset=-static_cast<int>(transition*v.height);
    return {v,offset,{0,0,v.width,std::max(0,v.height+offset)}};
}
}
