#pragma once
#include "ui/Scale.h"
#include "Geometry.h"
#include <cmath>
namespace launcher {
struct ListGeometry : Viewport { float transition=0,scroll=0; };
inline int rowSpacing(const Viewport& m) { return scaled(m,84); }
inline int listMargin(const Viewport& m) { return scaled(m,24); }
inline int iconRadius(const Viewport& m) { return scaled(m,34); }
inline Rect appsTarget(const Viewport& m) {
    return {m.width/2-scaled(m,64),m.height*3/4,scaled(m,128),scaled(m,70)};
}
// Icon circles are drawn with an even diameter (2r, not the 2r+1 of
// fillCircle), so their centre is the pixel boundary at (iconX, centerY).
// An even mask centres there exactly, with 10px of circle on all four sides,
// and 44px is the largest square whose corners stay inside the unselected
// radius 32 circle (they sit 31.1px from the centre).
inline constexpr int IconMaskPx=44;
// `RowLayout::radius` is the selected circle; an unselected row shrinks by this.
inline int selectionGrowth(const Viewport& m) { return scaled(m,2); }
// Leftmost stop of an icon centre, reached by a vertically centred row: it
// clears the bezel by `listMargin`.
inline int iconHomeX(const Viewport& m) { return listMargin(m)+iconRadius(m); }
// Icon centres all ride one arc. Its radius is 1.2x the panel's, so the path
// leaves the middle flatter than the bezel and closes on it towards the rim.
inline int arcRadius(const Viewport& m) {
    return std::max(1,static_cast<int>(std::lround(std::min(m.width,m.height)*0.6f)));
}
// Icon centre for a row sitting `dy` below the middle of the panel. The clamp
// only guards the square root; rows leave the screen well before the arc ends.
inline int iconCentreX(const Viewport& m,int dy) {
    const int r=arcRadius(m); const float d=std::min(r,std::abs(dy));
    return iconHomeX(m)+r-static_cast<int>(std::lround(std::sqrt(float(r)*r-d*d)));
}
inline int labelOffset(const Viewport& m) { return iconRadius(m)+scaled(m,14); }
// Widest a name can ever be, at the row's leftmost stop. Truncation is decided
// here and nowhere else, so the text never reflows while scrolling; a row swung
// out towards the rim is clipped by the bezel instead.
inline int labelWidth(const Viewport& m) {
    return std::max(0,m.width-listMargin(m)-iconHomeX(m)-labelOffset(m));
}
// iconX and centerY are boundary coordinates: the circle, the mask and the
// middle datum of the even height name font all centre on them.
struct RowLayout { Rect box{}; int iconX=0,centerY=0,radius=0,labelX=0; };
// Mask rectangle for an already scaled mask size, centred on the circle.
inline Rect iconBox(const RowLayout& r,int width,int height) {
    return {r.iconX-width/2,r.centerY-height/2,width,height};
}
// Shared by paint and hit testing. `box` runs from the icon to the right edge
// so it covers every pixel the row paints, including a name the bezel cuts off.
inline RowLayout layoutRow(const ListGeometry& m,int index) {
    const int offset=static_cast<int>((1-m.transition)*m.height);
    const int y=m.height/2+index*rowSpacing(m)-static_cast<int>(m.scroll)+offset;
    const int half=scaled(m,35);
    // Clipped by the incoming edge of the transition and by the panel, never by
    // `listMargin`: that is a radial clearance for the arc, and subtracting it
    // here as well would leave a black strip across the top and the bottom.
    Rect band=intersect({0,y-half,m.width,2*half+1},{0,offset,m.width,std::max(0,m.height-offset)});
    if (band.empty()) return {};
    const int iconR=iconRadius(m),iconX=iconCentreX(m,y-m.height/2);
    const int left=std::max(0,iconX-iconR-2);
    return {{left,band.y,m.width-left,band.h},iconX,y,iconR,iconX+labelOffset(m)};
}
inline int hitRow(const ListGeometry& m,int x,int y) {
    for (int i=0;i<5;++i) if (layoutRow(m,i).box.contains(x,y)) return i;
    return -1;
}
}
