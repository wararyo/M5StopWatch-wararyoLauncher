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
// Tapping here on the clock opens the list.
inline Rect appsTarget(const Viewport& m) {
    return {m.width/2-scaled(m,64),m.height*3/4,scaled(m,128),scaled(m,70)};
}
}
