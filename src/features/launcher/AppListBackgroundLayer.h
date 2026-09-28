#pragma once
#include "features/launcher/AppListLayout.h"
#include "ui/rendering/Renderer.h"
namespace launcher {
// The app list's opaque background, between the clock and the rows
// (docs/task10/plan-10-4.md 4). It covers everything below the list's
// incoming edge, so no face shows between the rows, in the colour the
// selected face asks for. Settings and the other screens never see it.
//
// It registers no element: a background as large as the panel would drag
// every change into a full repaint. It declares damage only for the pixels
// it actually changes (the strip its edge moved over, or all of it when the
// colour changes) and otherwise repaints inside what the frame restores. In
// the Renderer's black it changes nothing of its own, so it declares nothing:
// a face with a background declares what the edge covers and uncovers of it,
// and a black face (Digital) has nothing there but its elements.
class AppListBackgroundLayer final : public RenderLayer {
public:
    void prepare(Viewport viewport,float transition,bool visible,uint16_t color) {
        viewport_=viewport; transition_=transition; visible_=visible; color_=color;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
    // What the list covers this frame; empty while it is not shown.
    Rect area() const { return area_; }
private:
    Viewport viewport_{};
    float transition_=0;
    bool visible_=false;
    uint16_t color_=Renderer::Base;
    // The previous frame's, to find what changed.
    Rect area_{};
    uint16_t shown_=Renderer::Base;
    bool planned_=false;
};
}
