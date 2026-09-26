#pragma once
#include "features/home/HomeInteraction.h"
#include <algorithm>
#include <cmath>
namespace launcher {
enum class DigitalVariant : uint8_t { HourMinute,HourMinuteSecond };
// Digital's layout, as pure arithmetic over the viewport, the variant, how
// many background items it shows and what the fonts measure. The painter
// measures the fonts it actually loaded (or its fallbacks) and draws at these
// positions; the hit test reads the same APPS rectangle, so what is drawn and
// what a tap hits cannot drift apart. Positions follow the 466px reference
// (docs/Images/WatchFace/Digital*.png) scaled to the panel, adjusted on the
// device (2026-09-26): APPS 4px lower without items; with items the time 6px
// lower and the chips 6px higher. The fonts are fixed pixel sizes, so on
// another panel only their placement scales.
inline constexpr int DigitalMaxItems=2;
inline constexpr int DigitalReference=466;
// What the painter measured. The defaults are the embedded D-DIN-PRO subsets
// (docs/task10/fonts.md), which the host tests use.
struct DigitalMetrics {
    int timeAscent=71,timeDescent=1;
    // Widest advance any value of the group can take: HH over 00..23 and --,
    // mm and ss over 00..59 and --. The colon has one width.
    int hourWidth=114,minuteWidth=115,colonWidth=22;
    // How far the colons are raised. D-DIN-PRO sits its colon about 11px
    // below the middle of the digits; 8px up was chosen on the device
    // (2026-09-26). It stays inside the time's ascent.
    int colonLift=8;
    int textAscent=22,textDescent=5;    // 28px: the date
    int smallAscent=18,smallDescent=4;  // 22px: battery, APPS, labels
};
struct DigitalLayout {
    int cx=0;
    int batteryY=0;        // centre line of the battery row
    int dateBaseline=0;
    int timeBaseline=0;
    // The time in groups that tile its row without overlapping: HH with the
    // colon after it, mm (with the second colon when seconds show), ss. A new
    // second repaints ss alone, a new minute mm.
    Rect hour{},minute{},second{};
    int hourRight=0,colon1X=0,colon2X=0;
    int minuteX=0;         // left edge (HH:mm) or centre (HH:mm:ss) of mm
    int secondX=0;         // left edge of ss
    int chipY=0,chipHeight=0,chipGap=0,chipsWidth=0;  // chipsWidth: the most all may use
    Rect appsIcon{},apps{};
    int appsBaseline=0;
};
namespace digital {
inline int px(const Viewport& v,int reference) {
    return reference*std::min(v.width,v.height)/DigitalReference;
}
// Reference rows at 466px: without items, and moved up to make room for them.
inline int row(const Viewport& v,int plain,int withItems,int items) {
    return (v.height-std::min(v.width,v.height))/2+px(v,items ? withItems : plain);
}
}
// The APPS target: the icon and its label with room for a fingertip. It moves
// down with the information row, as the drawing does.
inline Rect digitalAppsTarget(const Viewport& v,int items) {
    const int cx=v.width/2,top=digital::row(v,354,369,items);
    return {cx-digital::px(v,64),top,digital::px(v,128),digital::px(v,70)};
}
inline DigitalLayout digitalLayout(const Viewport& v,DigitalVariant variant,int items,const DigitalMetrics& m={}) {
    using digital::px; using digital::row;
    items=std::clamp(items,0,DigitalMaxItems);
    DigitalLayout l;
    l.cx=v.width/2;
    l.batteryY=row(v,79,61,items);
    l.dateBaseline=row(v,126,108,items);
    l.timeBaseline=row(v,270,242,items);
    // A little room above and below the ink for the edges' coverage.
    const int margin=2,top=l.timeBaseline-m.timeAscent-margin,height=m.timeAscent+m.timeDescent+2*margin;
    const bool seconds=variant==DigitalVariant::HourMinuteSecond;
    const int width=m.hourWidth+m.colonWidth+m.minuteWidth+(seconds ? m.colonWidth+m.minuteWidth : 0);
    const int left=l.cx-width/2;
    l.hourRight=left+m.hourWidth;
    l.colon1X=l.hourRight;
    const int minuteLeft=l.colon1X+m.colonWidth;
    l.hour={left-margin,top,l.colon1X+m.colonWidth-(left-margin),height};
    if (seconds) {
        l.minuteX=minuteLeft+m.minuteWidth/2;
        l.colon2X=minuteLeft+m.minuteWidth;
        l.secondX=l.colon2X+m.colonWidth;
        l.minute={minuteLeft,top,m.minuteWidth+m.colonWidth,height};
        l.second={l.secondX,top,m.minuteWidth+margin,height};
    } else {
        l.minuteX=minuteLeft;
        l.minute={minuteLeft,top,m.minuteWidth+margin,height};
    }
    l.chipHeight=px(v,48); l.chipGap=px(v,16);
    l.chipY=row(v,312,312,items);
    // The widest the row of items may be where its far edge meets the circle,
    // keeping a margin from the rim.
    const float radius=std::min(v.width,v.height)/2.0f;
    const float far=std::abs(l.chipY+l.chipHeight/2.0f-v.height/2.0f);
    l.chipsWidth=std::max(0,int(2*std::sqrt(std::max(0.0f,radius*radius-far*far)))-2*px(v,12));
    const int icon=px(v,16);
    l.appsIcon={l.cx-icon/2,row(v,360,375,items),icon,icon};
    l.appsBaseline=row(v,401,416,items);
    l.apps=digitalAppsTarget(v,items);
    return l;
}
// Where the item chips go: centred as a row, left to right in the order given.
// Returns how many fit; a chip wider than its share is the caller's to shrink
// (chipWidthLimit) before it gets here.
inline int digitalChipWidthLimit(const DigitalLayout& l,int count) {
    count=std::clamp(count,1,DigitalMaxItems);
    return (l.chipsWidth-(count-1)*l.chipGap)/count;
}
inline int placeDigitalChips(const DigitalLayout& l,const int* widths,int count,Rect* out) {
    count=std::clamp(count,0,DigitalMaxItems);
    int total=count ? (count-1)*l.chipGap : 0;
    for (int i=0;i<count;++i) total+=widths[i];
    int x=l.cx-total/2;
    for (int i=0;i<count;++i) { out[i]={x,l.chipY-l.chipHeight/2,widths[i],l.chipHeight}; x+=widths[i]+l.chipGap; }
    return count;
}
// Digital's behaviour without its drawing: the variant, what a tap or a long
// press means, and when the face has to be drawn again. The variant outlives
// the face's caches (begin/end), so it survives leaving and coming back.
class DigitalControl {
public:
    DigitalVariant variant() const { return variant_; }
    // The frame about to be drawn; taps then hit what it shows.
    void update(const WatchData& d) { items_=std::min<int>(d.background.count,DigitalMaxItems); }
    int items() const { return items_; }
    HomeOutcome handle(const HomeEvent& e,const Viewport& v) {
        HomeOutcome out;
        if (e.kind==HomeEventKind::LongPress) {
            // Anywhere, APPS included: the press was not a tap.
            variant_=variant_==DigitalVariant::HourMinute ? DigitalVariant::HourMinuteSecond : DigitalVariant::HourMinute;
            out.changed=true;
        } else if (digitalAppsTarget(v,items_).contains(e.x,e.y)) out.request=HomeRequest::OpenAppList;
        return out;
    }
    TimeUs nextUpdate(TimeUs now,const WatchData& d) const {
        return variant_==DigitalVariant::HourMinuteSecond ? nextSecond(now,d) : nextMinute(now,d);
    }
    // The first two items, in the providers' order, are the ones drawn.
    BackgroundInterest backgroundInterest(const BackgroundSnapshot& s) const { return leadingItems(s,DigitalMaxItems); }
private:
    DigitalVariant variant_=DigitalVariant::HourMinute;
    int items_=0;
};
}
