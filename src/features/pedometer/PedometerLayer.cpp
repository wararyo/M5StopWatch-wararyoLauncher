#include "PedometerLayer.h"
#include "assets/AppIcons.h"
#include "i18n/Strings.h"
#include "ui/graphics/WatchFonts.h"
#include "features/pedometer/PedometerLayout.h"
#include <algorithm>
namespace launcher {
namespace {
// docs/Images/Pedometer in RGB565. OK is the lime the timer focuses with.
constexpr uint16_t Ink=0x0000,White=0xffff,Lime=0xc789; // #C4F24F
}
void PedometerLayer::plan(FramePlan& frame,Gfx&) {
    if (visible_!=shown_) {
        // Opened or closed: nothing here knows what was under it.
        frame.forceFull();
        for (auto& element:elements_) element=Element{};
        shown_=visible_;
    }
    if (!visible_) return;
    const Viewport& m=viewport_;
    char count[16];
    formatSteps(model_,count,sizeof(count));
    frame.add(elements_[Icon],pedometerIconBox(m),hashValue(1));
    frame.add(elements_[Title],pedometerTitleBox(m),hashString(text::StepsToday));
    frame.add(elements_[Count],pedometerCountBox(m),hashString(count,hashValue(uint32_t(model_.available))));
    frame.add(elements_[Ok],pedometerOkBox(m),hashValue(2));
}
void PedometerLayer::setNames(Gfx& g) const {
    const float scale=float(std::min(viewport_.width,viewport_.height))/468;
    g.setFont(names_ ? names_ : &fonts::lgfxJapanGothic_24); g.setTextSize(scale);
}
void PedometerLayer::paintCount(Gfx& g,const Rect& box) const {
    const Viewport& m=viewport_;
    char count[16];
    formatSteps(model_,count,sizeof(count));
    const int baseline=pedometerBaseline(m);
    // The figures and the unit are centred together; without an IMU the
    // dashes stand alone.
    setNames(g);
    const char* unit=model_.available ? text::StepsUnit : "";
    const int unitWidth=unit[0] ? g.textWidth(unit) : 0;
    const int gap=unit[0] ? pedometerUnitGap(m) : 0;
    if (digits_) {
        const int figures=digits_->width(count);
        const int left=box.x+(box.w-(figures+gap+unitWidth))/2;
        drawGlyphs(g,*digits_,count,left,baseline,White,Ink);
        if (unit[0]) {
            setNames(g);
            g.setTextDatum(baseline_left); g.setTextColor(White,Ink);
            g.drawString(unit,left+figures+gap,baseline);
        }
    } else {
        g.setFont(&fonts::FreeSansBold24pt7b); g.setTextSize(1);
        const int figures=g.textWidth(count);
        const int left=box.x+(box.w-(figures+gap+unitWidth))/2;
        g.setTextDatum(baseline_left); g.setTextColor(White,Ink);
        g.drawString(count,left,baseline);
        if (unit[0]) { setNames(g); g.drawString(unit,left+figures+gap,baseline); }
    }
    g.setTextSize(1);
}
void PedometerLayer::paint(Gfx& g,const PaintContext& context) {
    if (!visible_) return;
    const Viewport& m=viewport_;
    const Rect boxes[PartCount]={pedometerIconBox(m),pedometerTitleBox(m),pedometerCountBox(m),pedometerOkBox(m)};
    for (int part=0;part<PartCount;++part) {
        const Rect& b=boxes[part];
        if (!context.clip(g,b)) continue;
        // Every part owns its whole box, ground included.
        g.fillRect(b.x,b.y,b.w,b.h,Ink);
        switch (part) {
        case Icon:
            if (const auto* icon=appIcon(IconId::Pedometer))
                g.pushGrayscaleImage(b.x+(b.w-icon->width)/2,b.y+(b.h-icon->height)/2,icon->width,icon->height,
                                     icon->pixels,lgfx::grayscale_8bit,White,Ink);
            break;
        case Title:
            setNames(g);
            g.setTextDatum(middle_center); g.setTextColor(White,Ink);
            g.drawString(text::StepsToday,b.x+b.w/2,b.y+b.h/2);
            g.setTextSize(1);
            break;
        case Count:
            paintCount(g,b);
            break;
        case Ok: {
            g.fillSmoothRoundRect(b.x,b.y,b.w,b.h,pedometerCorner(m),Lime);
            const float scale=float(std::min(m.width,m.height))/468;
            g.setFont(text_ ? text_ : &fonts::FreeSansBold12pt7b); g.setTextSize(scale);
            g.setTextDatum(middle_center); g.setTextColor(Ink,Lime);
            g.drawString("OK",b.x+b.w/2,b.y+b.h/2);
            g.setTextSize(1);
            break;
        }
        }
    }
}
}
