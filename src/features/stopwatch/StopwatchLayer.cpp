#include "StopwatchLayer.h"
#include <algorithm>
#include <cstdio>
namespace launcher {
namespace {
// UserDemo's palette, converted to RGB565. The button label colours are its
// blend_in_difference(background, 0x858585).
constexpr uint16_t Panel=0x4249,Divider=0x5b2d,TimeInk=0xdf9f,NoLapInk=0x7410;
constexpr uint16_t LeftFill=0xb67f,StartFill=0x9f96,StopFill=0xfcf5;
constexpr uint16_t LeftInk=0x2a4f,StartInk=0x1366,StopInk=0x78c4;
}
void StopwatchLayer::build(Viewport m,const StopwatchModel& s) {
    count_=0;
    auto add=[&](Kind kind,Rect box,int index) -> Item& {
        Item& item=items_[count_++];
        item=Item{}; item.kind=kind; item.box=box; item.index=index;
        item.fill=Panel; item.ink=TimeInk;
        return item;
    };
    char clock[16],fraction[8];
    formatStopwatch(s.elapsedUs,clock,sizeof(clock),fraction,sizeof(fraction));
    std::snprintf(add(Clock,stopwatchClockBox(m),0).text,sizeof(Item::text),"%s",clock);
    std::snprintf(add(Fraction,stopwatchFractionBox(m),0).text,sizeof(Item::text),"%s",fraction);
    const bool running=s.state==StopwatchState::Running;
    Item& left=add(Button,stopwatchButtonBox(m,0),0);
    left.fill=LeftFill; left.ink=LeftInk;
    std::snprintf(left.text,sizeof(left.text),"%s",stopwatchLeftLabel(s.state));
    Item& right=add(Button,stopwatchButtonBox(m,1),1);
    right.fill=running ? StopFill : StartFill;
    right.ink=running ? StopInk : StartInk;
    std::snprintf(right.text,sizeof(right.text),"%s",stopwatchRightLabel(s.state));
    for (int i=0;i<StopwatchLapRows;++i) {
        // Row 0 carries the empty-list placeholder, so no extra element exists
        // just to say there is nothing yet.
        const bool filled=i<s.rows;
        Item& row=add(filled ? Lap : NoLap,stopwatchLapBox(m,i),i);
        if (filled) {
            char lap[16],hundredths[8];
            formatStopwatch(s.lapUs[i],lap,sizeof(lap),hundredths,sizeof(hundredths));
            std::snprintf(row.text,sizeof(row.text),"LAP %d",int(s.lapNumber[i]));
            std::snprintf(row.trailing,sizeof(row.trailing),"%s%s",lap,hundredths);
        } else if (i==0 && s.rows==0) {
            row.ink=NoLapInk;
            std::snprintf(row.text,sizeof(row.text),"-.-");
        }
    }
}
void StopwatchLayer::plan(FramePlan& frame,Gfx& g) {
    const Viewport& m=viewport_;
    count_=0;
    // Closed: register nothing. The screen change already forces a full repaint,
    // so there is no leftover to erase and the frame keeps its capacity free.
    if (!visible_) return;
    build(m,model_);
    // The elapsed time is sized to its own fixed box rather than to a guessed
    // multiplier, so the widest value it can hold always fits.
    g.setFont(&fonts::FreeSansBold24pt7b); g.setTextSize(1);
    const int natural=g.textWidth("00:00:00");
    const int room=stopwatchClockBox(m).w-2*stopwatchLapPadding(m);
    timeSize_=natural>0 ? float(room)/float(natural) : 1.0f;
    g.setTextSize(1);
    for (int i=0;i<Capacity;++i) {
        const auto& item=items_[i];
        uint32_t hash=hashString(item.text,hashValue(uint32_t(item.kind),0x9e3779b9u));
        hash=hashString(item.trailing,hashValue(uint32_t(item.fill),hash));
        frame.add(elements_[i],item.box,hash);
    }
}
void StopwatchLayer::paint(Gfx& g,const PaintContext& context) {
    if (!visible_) return;
    const Viewport& m=viewport_;
    const lgfx::IFont* font=font_;
    const float scale=float(std::min(m.width,m.height))/468;
    // The background, wherever the frame restored it: the whole of it on a
    // full repaint, the damage of a notice or a digit otherwise.
    const auto panel=stopwatchPanelBox(m);
    if (context.clip(g,panel))
        g.fillSmoothRoundRect(panel.x,panel.y,panel.w,panel.h,stopwatchPanelRadius(m),Panel);
    const auto divider=stopwatchDividerBox(m);
    if (context.clip(g,divider))
        g.fillRoundRect(divider.x,divider.y,divider.w,divider.h,divider.h/2,Divider);
    const int pad=stopwatchLapPadding(m);
    for (int i=0;i<count_;++i) {
        const auto& item=items_[i];
        if (!context.clip(g,item.box)) continue;
        const auto& b=item.box;
        switch (item.kind) {
        case Button:
            g.fillSmoothRoundRect(b.x,b.y,b.w,b.h,stopwatchButtonRadius(m),item.fill);
            g.setFont(&fonts::FreeSansBold12pt7b); g.setTextSize(scale);
            g.setTextDatum(middle_center); g.setTextColor(item.ink,item.fill);
            g.drawString(item.text,b.x+b.w/2,b.y+b.h/2);
            break;
        case Clock:
            // The panel colour across the whole box, as the text's background.
            g.fillRect(b.x,b.y,b.w,b.h,Panel);
            g.setFont(&fonts::FreeSansBold24pt7b); g.setTextSize(timeSize_);
            g.setTextDatum(middle_right); g.setTextColor(item.ink,Panel);
            // Right edge of the clock box IS the left edge of the hundredths,
            // so the two elements read as one string however the digits change.
            g.drawString(item.text,b.x+b.w,b.y+b.h/2);
            break;
        case Fraction:
            g.fillRect(b.x,b.y,b.w,b.h,Panel);
            g.setFont(&fonts::FreeSansBold24pt7b); g.setTextSize(timeSize_*0.62f);
            g.setTextDatum(middle_left); g.setTextColor(item.ink,Panel);
            g.drawString(item.text,b.x,b.y+b.h/2);
            break;
        case Lap:
            g.fillRect(b.x,b.y,b.w,b.h,Panel);
            g.setFont(font); g.setTextSize(scale);
            g.setTextColor(item.ink,Panel);
            g.setTextDatum(middle_left);
            g.drawString(item.text,b.x+pad,b.y+b.h/2);
            g.setTextDatum(middle_right);
            g.drawString(item.trailing,b.x+b.w-pad,b.y+b.h/2);
            break;
        case NoLap:
            g.fillRect(b.x,b.y,b.w,b.h,Panel);
            if (item.text[0]) {
                g.setFont(font); g.setTextSize(scale);
                g.setTextDatum(middle_center); g.setTextColor(item.ink,Panel);
                g.drawString(item.text,b.x+b.w/2,b.y+b.h/2);
            }
            break;
        }
    }
    g.setTextSize(1);
}
}
