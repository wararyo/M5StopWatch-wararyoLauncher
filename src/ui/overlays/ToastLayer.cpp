#include "ToastLayer.h"
#include "ui/rendering/Scale.h"
#include "ui/graphics/Text.h"
#include <cmath>
#include <cstdlib>
namespace launcher {
namespace {
constexpr uint16_t White=0xf7be,Panel=0x2104;
}
void ToastLayer::plan(FramePlan& frame,Gfx& g) {
    const Viewport& m=viewport_;
    const char* text=text_;
    const uint32_t hash=text ? hashString(text) : 0;
    if ((text!=nullptr)!=shown_ || (text && hash!=shownHash_)) frame.forceFull();
    shown_=text!=nullptr; shownHash_=hash;
    if (!text) { box_={}; fitted_[0]=0; handle_=frame.add(element_,{},0); return; }
    g.setFont(font_); g.setTextSize(float(std::min(m.width,m.height))/468);
    const int height=scaled(m,46),centreY=scaled(m,360);
    // Width follows the text: the settings notices are three times as long as
    // "準備中" and were being cut off by a box sized for the short one.
    // Clamped to the chord at the lower edge so the bezel never crops it.
    const int radius=std::min(m.width,m.height)/2;
    const int dy=std::abs(centreY+height/2-m.height/2);
    const int chord=int(std::sqrt(float(radius)*radius-float(dy)*dy))-scaled(m,6);
    const int width=std::min(2*chord,int(g.textWidth(text))+2*scaled(m,22));
    box_={m.width/2-width/2,centreY-height/2,width,height};
    // A notice too long even for the chord is shortened, never silently clipped.
    fitText(g,text,fitted_,sizeof(fitted_),box_.w-2*scaled(m,12));
    handle_=frame.add(element_,box_,hash);
    g.setTextSize(1);
}
void ToastLayer::paint(Gfx& g,const FramePlan& frame) {
    const Viewport& m=viewport_;
    if (box_.empty() || !frame.shouldPaint(handle_)) return;
    const auto& b=box_;
    g.setClipRect(b.x,b.y,b.w,b.h);
    g.fillRoundRect(b.x,b.y,b.w,b.h,scaled(m,14),Panel);
    g.setFont(font_); g.setTextSize(float(std::min(m.width,m.height))/468);
    g.setTextDatum(middle_center); g.setTextColor(White,Panel);
    g.drawString(fitted_,b.x+b.w/2,b.y+b.h/2);
    g.clearClipRect(); g.setTextSize(1);
}
}
