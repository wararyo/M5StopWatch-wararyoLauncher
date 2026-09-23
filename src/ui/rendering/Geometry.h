#pragma once
#include <algorithm>
namespace launcher {
struct Rect {
    int x=0,y=0,w=0,h=0;
    bool empty() const { return w<=0 || h<=0; }
    bool operator==(const Rect& b) const { return x==b.x && y==b.y && w==b.w && h==b.h; }
    bool operator!=(const Rect& b) const { return !(*this==b); }
    bool intersects(const Rect& b) const { return !empty() && !b.empty() && x<b.x+b.w && b.x<x+w && y<b.y+b.h && b.y<y+h; }
    bool contains(int px,int py) const { return !empty() && px>=x && px<x+w && py>=y && py<y+h; }
};
// Bounding box of the two. An empty operand contributes nothing, so a union
// over a mix of live and empty boxes is just a fold.
inline Rect unite(Rect a,Rect b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    const int x=std::min(a.x,b.x),y=std::min(a.y,b.y);
    return {x,y,std::max(a.x+a.w,b.x+b.w)-x,std::max(a.y+a.h,b.y+b.h)-y};
}
inline Rect intersect(Rect a,Rect b) {
    int x=std::max(a.x,b.x), y=std::max(a.y,b.y);
    return {x,y,std::max(0,std::min(a.x+a.w,b.x+b.w)-x),std::max(0,std::min(a.y+a.h,b.y+b.h)-y)};
}
}
