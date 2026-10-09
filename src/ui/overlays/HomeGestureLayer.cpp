#include "HomeGestureLayer.h"
#include <algorithm>
namespace launcher {
void HomeGestureLayer::plan(FramePlan& frame,Gfx&) {
    const int height=std::clamp(height_,0,viewport_.height);
    box_=height>0 ? Rect{0,0,viewport_.width,height} : Rect{};
    // The box carries the height; the colours are what else changes it.
    frame.add(element_,box_,height>0 ? (uint32_t(background_)<<16|foreground_) : 0);
}
void HomeGestureLayer::paint(Gfx& g,const PaintContext& context) {
    if (!context.clip(g,box_)) return;
    g.fillRect(box_.x,box_.y,box_.w,box_.h,background_);
    if (!icon_) return;
    // Centred on the band's middle row, so a low band cuts as much off the
    // icon's top as off its bottom. The clip is the band's.
    g.pushGrayscaleImage(box_.w/2-icon_->width/2,box_.h/2-icon_->height/2,icon_->width,icon_->height,
                         icon_->pixels,lgfx::grayscale_8bit,foreground_,background_);
}
}
