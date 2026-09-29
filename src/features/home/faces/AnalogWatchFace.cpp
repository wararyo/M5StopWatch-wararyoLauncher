#include "AnalogWatchFace.h"
#include "ui/graphics/Shapes.h"
#include "ui/graphics/WatchFonts.h"
#include <cstdio>
namespace launcher {
namespace {
// RGB565 of the reference colours: the dot (234,81,16), the date and the row
// (102,102,102).
constexpr uint16_t White=0xffff,Black=0x0000,Orange=0xea82,Grey=0x632c;
}
bool AnalogWatchFace::begin(Gfx& g,bool) {
    end();
    viewport_={int(g.width()),int(g.height())};
    // The date: Exp Bold 28px, as Digital's. Its digits (20px) are smaller
    // than the reference's (26px); the fonts are the existing ones
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
    std::printf("[WatchFace] analog fonts date=%s ascent=%d\n",watchTextFont() ? "vlw" : "builtin",date_.ascent);
    return true;
}
void AnalogWatchFace::end() {
    // Whether the dot shows, and the battery rule's state, are the face's,
    // not its resources': they stay.
    row_.end();
    elements_={};
}
void AnalogWatchFace::plan(FramePlan& frame,Gfx& g,const WatchEnvironment& env,const WatchData& d) {
    viewport_=env.viewport;
    layout_=analogLayout(viewport_,AnalogDateX);
    time_=analogTime(d);
    // The date: digits only, so the ink sits on the baseline.
    formatAnalogDate(time_,dateText_);
    g.setFont(date_.font); g.setTextSize(1);
    datePlace_=placeAnalogDate(layout_,g.textWidth(dateText_),date_.ascent,0);
    row_.plan(g,viewport_,layout_.row,d,control_.batteryShown(),control_.items());
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
}
void AnalogWatchFace::paint(Gfx& g,const PaintContext& context) {
    // Back to front, each inside its own box and the damage. The text is
    // drawn opaque on black before the hands, so a hand over it stays whole.
    for (int i=0;i<PartCount;++i) {
        if (!context.clip(g,boxes_[i])) continue;
        switch (i) {
        case Date:
            g.setFont(date_.font); g.setTextSize(1);
            g.setTextColor(Grey,Black); g.setTextDatum(baseline_left);
            g.drawString(dateText_,datePlace_.x,datePlace_.baseline);
            break;
        case Battery: row_.paint(g,InfoRowView::Battery,Grey,Black); break;
        case Item0: row_.paint(g,InfoRowView::Item0,Grey,Black); break;
        case Item1: row_.paint(g,InfoRowView::Item1,Grey,Black); break;
        default: {
            const auto& s=strokes_[i];
            drawWideLineClipped(g,s.a.x,s.a.y,s.b.x,s.b.y,s.r,i==Second ? Orange : White);
            break;
        }
        }
    }
    g.setTextSize(1);
}
}
