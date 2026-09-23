#pragma once
#include "ui/rendering/Geometry.h"
#include "ui/rendering/Scale.h"
#include "ui/rendering/Viewport.h"
#include <algorithm>
#include <cmath>
namespace launcher {
// Where a list sits this frame. The owner turns its own motion (the launcher's
// slide up over the clock, for one) into an offset and a clip; the list never
// sees why they are what they are.
struct ListPlacement {
    DrawRegion region{};
    float scroll=0;
};
// The whole panel, nothing sliding: a list at rest.
inline ListPlacement fullListPlacement(Viewport v,float scroll) {
    return {{v,0,{0,0,v.width,v.height}},scroll};
}
inline int rowSpacing(const Viewport& m) { return scaled(m,84); }
inline int rowHalfHeight(const Viewport& m) { return scaled(m,35); }
inline int listMargin(const Viewport& m) { return scaled(m,24); }
inline int iconRadius(const Viewport& m) { return scaled(m,34); }
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
// out towards the rim is clipped by the bezel instead. A row without an icon
// starts its text where the circle would have started, so it gets that room.
inline int labelWidth(const Viewport& m,bool icon=true) {
    const int start=icon ? iconHomeX(m)+labelOffset(m) : iconHomeX(m)-iconRadius(m);
    return std::max(0,m.width-listMargin(m)-start);
}
// iconX and centerY are boundary coordinates: the circle, the mask and the
// middle datum of the even height name font all centre on them.
struct RowLayout { Rect box{}; int iconX=0,centerY=0,radius=0,labelX=0; };
// Mask rectangle for an already scaled mask size, centred on the circle.
inline Rect iconBox(const RowLayout& r,int width,int height) {
    return {r.iconX-width/2,r.centerY-height/2,width,height};
}
inline int rowCentreY(const ListPlacement& p,int index) {
    const Viewport& m=p.region.viewport;
    return m.height/2+index*rowSpacing(m)-static_cast<int>(p.scroll)+p.region.offsetY;
}
// Shared by paint and hit testing. `box` runs from the icon to the right edge
// so it covers every pixel the row paints, including a name the bezel cuts
// off. It is the same with or without an icon, so both kinds hit the same.
inline RowLayout layoutListRow(const ListPlacement& p,int index,bool icon=true) {
    const Viewport& m=p.region.viewport;
    const int y=rowCentreY(p,index),half=rowHalfHeight(m);
    // Clipped by the owner's clip (the incoming edge of a transition) and by
    // the panel, never by `listMargin`: that is a radial clearance for the
    // arc, and subtracting it here as well would leave a black strip across
    // the top and the bottom.
    const Rect band=intersect({0,y-half,m.width,2*half+1},p.region.clip);
    if (band.empty()) return {};
    const int iconR=iconRadius(m),iconX=iconCentreX(m,y-m.height/2);
    const int left=std::max(0,iconX-iconR-2);
    const Rect box=intersect({left,band.y,m.width-left,band.h},p.region.clip);
    if (box.empty()) return {};
    return {box,iconX,y,iconR,icon ? iconX+labelOffset(m) : iconX-iconR};
}
// First and last rows that reach the clip; false when none does. Every row in
// between is visible too, because the clip is a single vertical interval.
inline bool visibleListRows(const ListPlacement& p,int count,int& first,int& last) {
    first=0; last=-1;
    if (count<=0 || p.region.clip.empty()) return false;
    const Viewport& m=p.region.viewport;
    const int spacing=rowSpacing(m),half=rowHalfHeight(m),top=rowCentreY(p,0);
    const int clipTop=p.region.clip.y,clipBottom=clipTop+p.region.clip.h;
    auto floorDiv=[](int a,int b) { return a>=0 ? a/b : -((-a+b-1)/b); };
    // Bounds that are off by at most one row, then tightened on the real
    // layout so the answer is exactly what layoutListRow draws.
    first=std::max(0,floorDiv(clipTop-half-top,spacing));
    last=std::min(count-1,floorDiv(clipBottom+half-top,spacing));
    while (first<=last && layoutListRow(p,first).box.empty()) ++first;
    while (last>=first && layoutListRow(p,last).box.empty()) --last;
    return first<=last;
}
inline int hitListRow(const ListPlacement& p,int count,int x,int y) {
    int first=0,last=-1;
    if (!visibleListRows(p,count,first,last)) return -1;
    for (int i=first;i<=last;++i) if (layoutListRow(p,i).box.contains(x,y)) return i;
    return -1;
}
// Most rows a clip as tall as the panel can show at once: row centres are
// `rowSpacing` apart and a row stays visible while its centre is within
// `rowHalfHeight` of the clip.
inline int maxVisibleListRows(const Viewport& m) {
    const int spacing=rowSpacing(m);
    return (m.height+2*rowHalfHeight(m)+spacing-1)/spacing;
}
// Display slots a ListView keeps, independent of how many rows the data has.
// A square panel shows at most 7 rows (84px apart, 71px tall, over 468px at
// every scale); one spare covers rounding. A panel taller than it is wide can
// need more, and then the view draws every visible row directly on a full
// repaint instead of dropping any (ListView::plan).
inline constexpr int ListVisibleSlots=8;
}
