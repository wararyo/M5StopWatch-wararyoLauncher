#include "Renderer.h"
#include <cstdio>
namespace launcher {
bool Renderer::draw(RenderLayer* const* layers,int count,FrameOverlay* overlay) {
    ++layouts_;
    frame_.begin(full_,capacity_);
    for(int i=0;i<count;++i) layers[i]->plan(frame_,display_);
    frame_.resolve();
    const bool painted=frame_.anyPaint();
    lastDirty_=!painted ? Rect{} : frame_.full() ? Rect{0,0,int(display_.width()),int(display_.height())}
                                                 : frame_.dirtyBounds();
    if(painted) {
        display_.startWrite(); display_.clearClipRect();
        if(frame_.full()) display_.fillScreen(0);
        else for(int i=0;i<frame_.count();++i) {
            const auto r=frame_.eraseBox(i);
            if(!r.empty()) display_.fillRect(r.x,r.y,r.w,r.h,0);
        }
        // Full fallback paints every layer, even those whose add() returned -1.
        for(int i=0;i<count;++i) layers[i]->paint(display_,frame_);
        // Last of all, and outside the plan: the overlay owns its rectangle
        // and is told what the frame touched, so it can leave its pixels
        // alone when nothing reached them.
        if(overlay)
            overlay->paint(display_,frame_.full() ? Rect{0,0,int(display_.width()),int(display_.height())}
                                                  : frame_.dirtyBounds());
        display_.endWrite(); ++paints_;
    }
    if(frame_.overflow() && !overflowReported_) std::printf("[Renderer] element capacity exceeded: full repaint\n");
    overflowReported_=frame_.overflow();
    full_=frame_.overflow();
    return painted;
}
}
