#include "AppListBackgroundLayer.h"
namespace launcher {
void AppListBackgroundLayer::plan(FramePlan& frame,Gfx&) {
    const Rect area=visible_ ? appListCover(viewport_,transition_) : Rect{};
    // Black over black changes nothing of its own: what a face showed under
    // the edge is the face's to declare (WatchFace::plan).
    if (!planned_) frame.damage(area);
    else if (color_!=Renderer::Base || shown_!=Renderer::Base)
        frame.damage(appListCoverChange(area_,shown_,area,color_));
    area_=area; shown_=color_; planned_=true;
}
void AppListBackgroundLayer::paint(Gfx& g,const PaintContext& context) {
    // Black is what the Renderer has just restored here: nothing to add.
    if (color_==Renderer::Base || !context.clip(g,area_)) return;
    g.fillRect(area_.x,area_.y,area_.w,area_.h,color_);
}
}
