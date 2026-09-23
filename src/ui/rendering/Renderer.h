#pragma once
#include "ui/rendering/Element.h"
#include "ui/graphics/Gfx.h"
#include <cstdint>
namespace launcher {
// One part of a frame. Its input is fixed before the frame starts and does not
// change until paint() returns, so every layer plans and paints the same frame.
// plan() registers its elements; paint() draws those the plan says to, or all
// of them on a full repaint, including any whose add() returned -1.
class RenderLayer {
public:
    virtual ~RenderLayer()=default;
    virtual void plan(FramePlan& frame,Gfx& g)=0;
    virtual void paint(Gfx& g,const FramePlan& frame)=0;
};
// Painted after every layer and before the transfer, outside the plan: it
// restores its own pixels instead of taking part in the overlap resolution.
// `dirty` is everything the frame erased and repainted.
class FrameOverlay {
public:
    virtual ~FrameOverlay()=default;
    virtual void paint(Gfx& g,const Rect& dirty)=0;
};
// Runs one frame over the layers it is handed, back to front: every plan, the
// resolution, every erase, every paint, the overlay, then the transfer. It
// knows nothing of what the layers show, which screen is open or why a frame
// has to be repainted in full; whoever composes the frame decides that and
// calls invalidate().
class Renderer {
public:
    explicit Renderer(Gfx& display):display_(display) {}
    // The next frame repaints everything: after a wake, or when the owner's
    // composition changed so that layers have no history to erase with.
    void invalidate() { full_=true; }
    // True when anything reached the panel. Returns after endWrite, where this
    // panel flushes, so a time taken right after covers the transfer too.
    bool draw(RenderLayer* const* layers,int count,FrameOverlay* overlay=nullptr);
    uint32_t layouts() const { return layouts_; }
    uint32_t paints() const { return paints_; }
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    void capacityForTest(int n) { capacity_=n; invalidate(); }
#endif
private:
    Gfx& display_;
    FramePlan frame_;
    bool full_=true,overflowReported_=false;
    int capacity_=FramePlan::Capacity;
    uint32_t layouts_=0,paints_=0;
};
}
