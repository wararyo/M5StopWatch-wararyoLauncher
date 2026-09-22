#include "Element.h"
namespace launcher {
int FramePlan::add(Element& element,Rect box,uint32_t fingerprint) {
    if(count_>=limit_) { overflow_=true; full_=true; return -1; }
    const int handle=count_++;
    entries_[handle]={&element,box,element.valid ? element.box : Rect{},fingerprint,
        full_ || !element.valid || box!=element.box || fingerprint!=element.fingerprint};
    return handle;
}
void FramePlan::resolve() {
    if(full_) for(int i=0;i<count_;++i) entries_[i].paint=true;
    // Close over old erasures AND new paints. A repaint may overwrite a stable
    // foreground, which itself can overlap another element. Preserve z-order.
    bool changed=true;
    while(changed) {
        changed=false;
        for(int i=0;i<count_;++i) if(entries_[i].paint)
            for(int j=0;j<count_;++j) if(!entries_[j].paint &&
                (entries_[j].box.intersects(entries_[i].oldBox) ||
                 entries_[j].box.intersects(entries_[i].box) ||
                 entries_[j].oldBox.intersects(entries_[i].oldBox))) {
                entries_[j].paint=true; changed=true;
            }
    }
    dirty_={};
    for(int i=0;i<count_;++i) {
        auto& e=entries_[i];
        if(e.paint) dirty_=unite(dirty_,unite(e.box,e.oldBox));
        *e.element={e.box,e.fingerprint,true};
    }
}
bool FramePlan::anyPaint() const {
    if(full_) return true;
    for(int i=0;i<count_;++i) if(entries_[i].paint && (!entries_[i].box.empty() || !entries_[i].oldBox.empty())) return true;
    return false;
}
}
