#include "NoonishWatchFace.h"
#include <algorithm>
namespace launcher {
void NoonishWatchFace::end() {
    HandsFace::end();
    planned_=false;
}
void NoonishWatchFace::planBackground(FramePlan& frame,const WatchEnvironment& env) {
    clip_=env.clip;
    regions_.split=noonishSplit(layout(),time());
    // The regions are background, not an element (docs/task10/plan-10-4.md 1).
    // A new step turns them: all of the face that shows, then and now. The
    // list's edge moving uncovers or covers the band between its two places.
    const bool turned=regions_.split.step!=shownStep_ || env.viewport.width!=shownViewport_.width ||
        env.viewport.height!=shownViewport_.height;
    if (planned_ && turned) frame.damage(unite(clip_,shownClip_));
    else if (planned_ && clip_!=shownClip_) {
        const int a=shownClip_.y+shownClip_.h,b=clip_.y+clip_.h;
        frame.damage({0,std::min(a,b),env.viewport.width,std::abs(a-b)});
    }
    planned_=true; shownStep_=regions_.split.step; shownViewport_=env.viewport; shownClip_=clip_;
}
void NoonishWatchFace::paintBackground(Gfx& g,const PaintContext& context) {
    if (!context.clip(g,clip_)) return;
    const Rect area=intersect(context.area(),clip_);
    // Row by row, as runs of one colour: a row crosses each line once, so it
    // is a few runs and the blended pixels along the boundaries.
    for (int y=area.y;y<area.y+area.h;++y)
        noonishRow(regions_.split,y,area.x,area.x+area.w,[&](int x,int length,uint16_t colour) {
            g.fillRect(x,y,length,1,colour);
        });
}
void NoonishWatchFace::paintDot(Gfx& g,const AnalogStroke& s) {
    // Each pixel from the background under it, never from what the last frame
    // left there, so the dot does not whiten as it is drawn again. Nothing
    // else reaches the orbit, so there is nothing else to blend with.
    const Rect box=strokeBounds(s);
    uint16_t colour;
    for (int y=box.y;y<box.y+box.h;++y) for (int x=box.x;x<box.x+box.w;++x)
        if (noonishDot(regions_.split,s,x,y,colour)) g.drawPixel(x,y,colour);
}
}
