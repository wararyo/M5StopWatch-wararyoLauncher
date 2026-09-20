#pragma once
#include "DisplayModel.h"
#include "Geometry.h"
#include <cmath>
namespace launcher {
inline int scaled(const ScreenModel& m,int px) { return std::max(1,px*std::min(m.width,m.height)/468); }
inline int rowSpacing(const ScreenModel& m) { return scaled(m,84); }
inline Rect appsTarget(const ScreenModel& m) {
    return {m.width/2-scaled(m,64),m.height*3/4,scaled(m,128),scaled(m,70)};
}
// Icon circles are drawn with an even diameter (2r, not the 2r+1 of
// fillCircle), so their centre is the pixel boundary at (iconX, centerY).
// An even mask centres there exactly, with 10px of circle on all four sides,
// and 44px is the largest square whose corners stay inside the unselected
// radius 32 circle (they sit 31.1px from the centre).
inline constexpr int IconMaskPx=44;
// `RowLayout::radius` is the selected circle; an unselected row shrinks by this.
inline int selectionGrowth(const ScreenModel& m) { return scaled(m,2); }
// iconX and centerY are boundary coordinates: the circle, the mask and the
// middle datum of the even height name font all centre on them.
struct RowLayout { Rect box{}; int iconX=0,centerY=0,radius=0,labelX=0; };
// Mask rectangle for an already scaled mask size, centred on the circle.
inline Rect iconBox(const RowLayout& r,int width,int height) {
    return {r.iconX-width/2,r.centerY-height/2,width,height};
}
// Shared by paint and hit testing. Row corners stay inside the circle.
inline RowLayout layoutRow(const ScreenModel& m,int index) {
    const int offset=static_cast<int>((1-m.transition)*m.height);
    const int y=m.height/2+index*rowSpacing(m)-static_cast<int>(m.scroll)+offset;
    const int half=scaled(m,35),margin=scaled(m,12);
    const int top=std::max(margin,offset);
    Rect vertical=intersect({0,y-half,m.width,2*half+1},{0,top,m.width,std::max(0,m.height-margin-top)});
    if (vertical.empty()) return {};
    const int radius=std::min(m.width,m.height)/2;
    const int farY=std::max(std::abs(vertical.y-m.height/2),std::abs(vertical.y+vertical.h-m.height/2));
    const int chord=static_cast<int>(std::sqrt(std::max(0,radius*radius-farY*farY)));
    const int left=m.width/2-chord+margin;
    Rect box{left,vertical.y,std::max(0,2*(chord-margin)),vertical.h};
    const int iconR=scaled(m,34);
    return {box,left+iconR+2,y,iconR,left+2*iconR+scaled(m,14)};
}
inline int hitRow(const ScreenModel& m,int x,int y) {
    for (int i=0;i<5;++i) if (layoutRow(m,i).box.contains(x,y)) return i;
    return -1;
}
}
