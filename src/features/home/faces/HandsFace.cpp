#include "HandsFace.h"
#include "ui/graphics/Shapes.h"
#include "ui/graphics/WatchFonts.h"
#include <cstdio>
namespace launcher {
namespace {
constexpr uint16_t White=0xffff;
}
bool HandsFace::begin(Gfx& g,bool) {
    end();
    // The date: Exp Bold 28px, as Digital's. Its digits (20px) are smaller
    // than the references' (26px); the fonts are the existing ones
    // (docs/task11/plan.md 3).
    date_.font=watchTextFont() ? watchTextFont() : &fonts::FreeSans18pt7b;
    g.setFont(date_.font); g.setTextSize(1);
    if (date_.font->getType()==lgfx::IFont::ft_vlw) {
        date_.ascent=static_cast<const lgfx::VLWfont*>(date_.font)->maxAscent;
    } else {
        lgfx::FontMetrics m{}; date_.font->getDefaultMetric(&m);
        date_.ascent=m.baseline;
    }
    row_.begin(g);
    std::printf("[WatchFace] %s fonts date=%s ascent=%d\n",id(),watchTextFont() ? "vlw" : "builtin",date_.ascent);
    return true;
}
void HandsFace::end() {
    // Whether the dot shows, and the battery rule's state, are the face's,
    // not its resources': they stay.
    row_.end();
    elements_={};
}
void HandsFace::plan(FramePlan& frame,Gfx& g,const WatchEnvironment& env,const WatchData& d) {
    layout_=analogLayout(env.viewport,dateX_);
    time_=analogTime(d);
    // The date: digits only, so the ink sits on the baseline.
    formatAnalogDate(time_,dateText_);
    g.setFont(date_.font); g.setTextSize(1);
    datePlace_=placeAnalogDate(layout_,g.textWidth(dateText_),date_.ascent,0);
    row_.plan(g,env.viewport,layout_.row,d,control_.batteryShown(),control_.items());
    // The hands and the dot only while the time is known; the hub always.
    const bool valid=time_.valid;
    strokes_[Hour]=analogHour(layout_,time_);
    strokes_[Minute]=analogMinute(layout_,time_);
    strokes_[Hub]=analogHub(layout_);
    strokes_[Second]=analogSecond(layout_,time_);
    boxes_[Date]=datePlace_.box;
    for (int i=InfoRowView::Battery;i<InfoRowView::PartCount;++i) boxes_[Battery+i]=row_.box(i);
    boxes_[Hour]=valid ? strokeBounds(strokes_[Hour]) : Rect{};
    boxes_[Minute]=valid ? strokeBounds(strokes_[Minute]) : Rect{};
    boxes_[Hub]=strokeBounds(strokes_[Hub]);
    boxes_[Second]=valid && control_.seconds() ? strokeBounds(strokes_[Second]) : Rect{};
    // A hand can turn inside the same box: its key says that it moved.
    const uint32_t hashes[PartCount]={
        hashString(dateText_),row_.key(InfoRowView::Battery),row_.key(InfoRowView::Item0),row_.key(InfoRowView::Item1),
        hashValue(uint32_t(time_.hourKey())),hashValue(uint32_t(time_.minuteKey())),0x4b1bu,
        hashValue(uint32_t(control_.seconds() ? time_.secondKey() : 0))};
    for (int i=0;i<PartCount;++i) frame.add(elements_[i],intersect(boxes_[i],env.clip),hashes[i]);
    planBackground(frame,env);
}
void HandsFace::paint(Gfx& g,const PaintContext& context) {
    paintBackground(g,context);
    // Back to front, each inside its own box and the damage.
    const Backdrop& back=backdrop();
    for (int i=0;i<PartCount;++i) {
        if (!context.clip(g,boxes_[i])) continue;
        switch (i) {
        case Date:
            g.setFont(date_.font); g.setTextSize(1);
            setTextInk(g,ink_,back); g.setTextDatum(baseline_left);
            g.drawString(dateText_,datePlace_.x,datePlace_.baseline);
            break;
        case Battery: row_.paint(g,InfoRowView::Battery,ink_,back); break;
        case Item0: row_.paint(g,InfoRowView::Item0,ink_,back); break;
        case Item1: row_.paint(g,InfoRowView::Item1,ink_,back); break;
        case Second: paintDot(g,strokes_[i]); break;
        default: {
            const auto& s=strokes_[i];
            drawWideLineClipped(g,s.a.x,s.a.y,s.b.x,s.b.y,s.r,White);
            break;
        }
        }
    }
    g.setTextSize(1);
}
}
