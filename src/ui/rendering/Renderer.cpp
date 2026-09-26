#include "Renderer.h"
#include <cstdio>
namespace launcher {
bool Renderer::draw(RenderLayer* const* layers,int count,FrameOverlay* overlay) {
    ++layouts_;
    frame_.begin(full_,{0,0,int(display_.width()),int(display_.height())},capacity_);
    for(int i=0;i<count;++i) layers[i]->plan(frame_,display_);
    frame_.resolve();
    const bool painted=frame_.anyPaint();
    const Rect area=frame_.area();
    lastDirty_=painted ? area : Rect{};
    if(painted) {
        display_.startWrite();
        const PaintContext context(frame_,area);
        // The base first, then every layer inside the same rectangle. A layer
        // may narrow the clip; the next one starts from the area again.
        context.restore(display_);
        display_.fillRect(area.x,area.y,area.w,area.h,Base);
        for(int i=0;i<count;++i) { context.restore(display_); layers[i]->paint(display_,context); }
        // Last of all, and outside the plan: the overlay owns its rectangle
        // and is told what the frame touched, so it can leave its pixels
        // alone when nothing reached them.
        display_.clearClipRect();
        if(overlay) overlay->paint(display_,area);
        display_.endWrite(); ++paints_;
    }
    if(frame_.overflow() && !overflowReported_) std::printf("[Renderer] element capacity exceeded: full repaint\n");
    overflowReported_=frame_.overflow();
    full_=frame_.overflow();
    return painted;
}
}
