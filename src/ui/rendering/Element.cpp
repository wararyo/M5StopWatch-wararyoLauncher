#include "Element.h"
namespace launcher {
int FramePlan::add(Element& element,Rect box,uint32_t fingerprint) {
    if(count_>=limit_) { overflow_=true; full_=true; return -1; }
    const int handle=count_++;
    entries_[handle]={&element,box,element.valid ? element.box : Rect{},fingerprint,
        !element.valid || box!=element.box || fingerprint!=element.fingerprint,false};
    return handle;
}
void FramePlan::resolve() {
    // The damage comes from what changed only. An element that merely
    // overlaps it is repainted inside it and adds nothing, so the rectangle
    // never grows by chains of overlaps.
    damage_={};
    if(!full_) {
        damage_=declared_;
        for(int i=0;i<count_;++i) if(entries_[i].changed)
            damage_=unite(damage_,unite(entries_[i].box,entries_[i].oldBox));
        damage_=intersect(damage_,bounds_);
        if(damage_.empty()) damage_={};
    }
    const Rect area=this->area();
    for(int i=0;i<count_;++i) {
        auto& e=entries_[i];
        e.paint=e.box.intersects(area);
        *e.element={e.box,e.fingerprint,true};
    }
}
}
