#pragma once
#include "ui/rendering/Element.h"
namespace launcher {
// Where a layer may draw in this frame (docs/task10/plan-10-4.md 3): the area
// the frame restores (its damage, or the whole screen), narrowed by the
// owner to the part the layer is shown in (the clock's uncovered clip, say).
//
// Every drawing call goes through clip(): it sets the clip to the three-way
// intersection of an element's box, the damage and the layer's clip, and
// says whether anything is left. A layer never clears the clip to draw more,
// since pixels outside the damage are the previous frame's and stay; the
// Renderer puts the clip back to the area between layers.
class PaintContext {
public:
    PaintContext(const FramePlan& frame,Rect area):frame_(&frame),area_(area) {}
    // The same frame, drawn only inside `clip` as well.
    PaintContext within(Rect clip) const { return {*frame_,intersect(area_,clip)}; }
    const FramePlan& frame() const { return *frame_; }
    Rect area() const { return area_; }
    bool full() const { return frame_->full(); }
    bool touches(Rect box) const { return area_.intersects(box); }
    // False, with the clip left alone, when the box does not reach the area.
    template<class G> bool clip(G& g,Rect box) const {
        const Rect r=intersect(box,area_);
        if (r.empty()) return false;
        g.setClipRect(r.x,r.y,r.w,r.h);
        return true;
    }
    // Back to the whole area, after a narrower clip.
    template<class G> void restore(G& g) const {
        if (area_.empty()) g.setClipRect(0,0,0,0); else g.setClipRect(area_.x,area_.y,area_.w,area_.h);
    }
private:
    const FramePlan* frame_;
    Rect area_{};
};
}
