#pragma once
#include "ui/rendering/Element.h"
#include "ui/rendering/PaintContext.h"
#include "ui/graphics/Gfx.h"
#include <cstdint>
#include <optional>
namespace launcher {
// What a frame lies on unless its owner says otherwise (Renderer::setBase).
inline constexpr uint16_t DefaultBase=0x0000;
// One part of a frame. Its input is fixed before the frame starts and does not
// change until paint() returns, so every layer plans and paints the same frame.
// plan() registers its elements and declares any background change as
// damage; paint() redraws, back to front, whatever of its background and
// elements reaches the context's area, through PaintContext::clip. On a full
// repaint the area is the screen, so elements whose add() returned -1 are
// drawn too.
class RenderLayer {
public:
    virtual ~RenderLayer()=default;
    virtual void plan(FramePlan& frame,Gfx& g)=0;
    virtual void paint(Gfx& g,const PaintContext& context)=0;
    // Where this frame's paint covers every pixel with opaque colour of its
    // own, whatever lay there (a face's scenery). Asked of the back-most
    // layer after plan: the Renderer does not restore its base there, since
    // nothing of it would show.
    virtual Rect opaqueArea() const { return {}; }
    // The colour this layer's screen lies on, asked of the screen shown
    // (HostRenderer::draw) and restored as the frame's base, so the layer has
    // no background of its own to fill. A screen whose list draws names on it
    // hands its ListView the same colour.
    virtual uint16_t background() const { return DefaultBase; }
};
// Painted after every layer and before the transfer, outside the plan: it
// restores its own pixels instead of taking part in the damage.
// `dirty` is everything the frame restored and repainted.
class FrameOverlay {
public:
    virtual ~FrameOverlay()=default;
    virtual void paint(Gfx& g,const Rect& dirty)=0;
};
// Runs one frame over the layers it is handed, back to front: every plan, the
// damage, the base inside it, every paint clipped to it, the overlay, then the
// transfer. It knows nothing of what the layers show, which screen is open or
// why a frame has to be repainted in full; whoever composes the frame decides
// that, calls invalidate() and sets the base.
class Renderer {
public:
    explicit Renderer(Gfx& display):display_(display) {}
    // What the Renderer restores before any layer paints, except where the
    // back-most layer paints opaque. A layer whose background is this colour
    // has nothing of its own to fill. A new base changes every pixel nothing
    // covers, so the frame it arrives with is a full repaint.
    void setBase(uint16_t color) { if (color!=base_) { base_=color; full_=true; } }
    uint16_t base() const { return base_; }
    // The next frame repaints everything: after a wake, or when the owner's
    // composition changed so that layers have no history to compare with.
    void invalidate() { full_=true; }
    // The part of the display frames are drawn in, when the panel cannot take
    // all of it (HostRenderer::begin). Nothing outside it is ever written, so
    // no transfer reaches it; a full repaint covers this area.
    void limitTo(Rect area) { limit_=area; invalidate(); }

    // True when anything reached the panel. Returns after endWrite, where this
    // panel flushes, so a time taken right after covers the transfer too.
    //
    // `within`, when given, is where this one frame may draw: the frame's area
    // is the panel's lasting limit (limitTo) and it together. Damage is clipped
    // to it, a full repaint covers just it, and the overlay writes nothing
    // outside it; what lies outside stays exactly as the panel holds it (the
    // home gesture's clock coming in over a screen, docs/task14/plan.md 2.6).
    // An empty one draws nothing, and a full repaint asked for waits for a
    // frame that has room. Without it the frame has the whole limit.
    bool draw(RenderLayer* const* layers,int count,FrameOverlay* overlay=nullptr,std::optional<Rect> within={});
    uint32_t layouts() const { return layouts_; }
    // What the last frame sent to the panel: the whole screen, its damage,
    // or nothing.
    Rect lastDirty() const { return lastDirty_; }
    uint32_t paints() const { return paints_; }
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    void capacityForTest(int n) { capacity_=n; invalidate(); }
#endif
private:
    Gfx& display_;
    FramePlan frame_;
    bool full_=true,overflowReported_=false;
    int capacity_=FramePlan::Capacity;
    uint16_t base_=DefaultBase;
    uint32_t layouts_=0,paints_=0;
    Rect lastDirty_{},limit_{};
};
}
