#pragma once
#include "ui/graphics/Gfx.h"
#include "ui/graphics/IconBitmap.h"
#include "ui/rendering/Renderer.h"
#include "ui/rendering/Viewport.h"
namespace launcher {
// The band at the top of the panel that shows home on its way
// (docs/task14/plan.md 2.2): the full width down to its height, the face's
// ground, and the face icon centred in it, cut at the band's edges while the
// band is lower than the icon. Over every screen and below the notice. What
// the band is for and how tall it is are the system's; this only draws it.
//
// The band is one element: as it grows or shrinks, the frame restores the
// rows between its old and new edge and whatever lies under them repaints.
class HomeGestureLayer final : public RenderLayer {
public:
    // `icon` is borrowed for the life of the app (assets/AppIcons.h); null
    // draws the band alone. A height of 0 shows no band.
    void prepare(Viewport viewport,int height,uint16_t background,uint16_t foreground,const IconBitmap* icon) {
        viewport_=viewport; height_=height; background_=background; foreground_=foreground; icon_=icon;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
private:
    Viewport viewport_{};
    int height_=0;
    uint16_t background_=0,foreground_=0xffff;
    const IconBitmap* icon_=nullptr;
    Element element_;
    Rect box_{};
};
}
