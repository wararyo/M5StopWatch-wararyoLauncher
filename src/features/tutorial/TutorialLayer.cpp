#include "TutorialLayer.h"
#include "features/tutorial/TutorialLayout.h"
#include "i18n/Strings.h"
#include "ui/graphics/Shapes.h"
#include <algorithm>
namespace launcher {
namespace {
// The settings editor's palette, so both steps read as one screen.
constexpr uint16_t White=0xf7be,Muted=0xad75,Lime=0xb7e0,Ink=0x0000;
constexpr const char* Lines[TutorialLines]={text::TutorialLine1,text::TutorialLine2,text::TutorialLine3};
}
Rect TutorialLayer::box(int part) const {
    if (part==Mark) return tutorialMarkBox(viewport_);
    if (part==Title) return tutorialTitleBox(viewport_);
    return tutorialLineBox(viewport_,part-Line);
}
const char* TutorialLayer::label(int part) const {
    if (part==Title) return text::TutorialTitle;
    return part>=Line ? Lines[part-Line] : "";
}
void TutorialLayer::plan(FramePlan& frame,Gfx&) {
    const Shown shown=!visible_ ? Shown::None
        : model_.step==TutorialStep::DateTime ? Shown::DateTime : Shown::Guide;
    if (shown!=shown_) {
        // Opened, closed, or the other step: nothing here knows what lay
        // under these pixels, so the frame is repainted in full and neither
        // step compares against the other's boxes.
        frame.forceFull();
        editor_.invalidate();
        for (auto& element:elements_) element=Element{};
        shown_=shown;
    }
    if (shown==Shown::None) return;
    if (shown==Shown::DateTime) { editor_.plan(frame,viewport_,model_.editor,false); return; }
    for (int part=0;part<PartCount;++part)
        frame.add(elements_[part],box(part),hashString(label(part),hashValue(uint32_t(part))));
}
void TutorialLayer::paint(Gfx& g,const PaintContext& context) {
    if (shown_==Shown::None) return;
    if (shown_==Shown::DateTime) { editor_.paint(g,context); return; }
    const float scale=float(std::min(viewport_.width,viewport_.height))/468;
    for (int part=0;part<PartCount;++part) {
        const Rect b=box(part);
        if (!context.clip(g,b)) continue;
        // Every part owns its whole box, ground included.
        g.fillRect(b.x,b.y,b.w,b.h,Ink);
        if (part==Mark) {
            // A chevron pointing down, where the swipe begins.
            const int cx=b.x+b.w/2,top=b.y+b.h/4,bottom=b.y+b.h*3/4;
            const float r=std::max(1.0f,float(b.h)/10);
            // Clipped: Gfx::drawWideLine would paint outside the damage (ui/graphics/Gfx.h).
            drawWideLineClipped(g,b.x+b.w/5,top,cx,bottom,r,Lime);
            drawWideLineClipped(g,b.x+b.w-b.w/5,top,cx,bottom,r,Lime);
            continue;
        }
        g.setFont(font_ ? font_ : &fonts::lgfxJapanGothic_24); g.setTextSize(scale);
        g.setTextDatum(middle_center); g.setTextColor(part==Title ? Muted : White,Ink);
        g.drawString(label(part),b.x+b.w/2,b.y+b.h/2);
        g.setTextSize(1);
    }
}
}
