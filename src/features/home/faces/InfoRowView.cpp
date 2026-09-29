#include "InfoRowView.h"
#include "features/background/BackgroundInfoHub.h"
#include "ui/graphics/MaskImage.h"
#include "ui/graphics/Shapes.h"
#include "ui/graphics/Text.h"
#include "ui/graphics/VlwFont.h"
#include "ui/graphics/WatchFonts.h"
#include "ui/rendering/Element.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace launcher {
namespace {
bool ascii(const char* s) {
    for (;*s;++s) if (static_cast<unsigned char>(*s)>=0x80) return false;
    return true;
}
// Room around a group for its edges' coverage.
constexpr int Pad=2;
int px(const Viewport& v,float reference) { return int(reference*std::min(v.width,v.height)/InfoRowReference); }
}
void InfoRowView::begin(Gfx& g) {
    auto pick=[&](Font& f,const lgfx::IFont* embedded,const lgfx::IFont* fallback) {
        f.font=embedded ? embedded : fallback;
        g.setFont(f.font); g.setTextSize(1);
        if (f.font->getType()==lgfx::IFont::ft_vlw) {
            const auto* v=static_cast<const lgfx::VLWfont*>(f.font);
            f.ascent=v->maxAscent; f.descent=v->maxDescent;
        } else {
            lgfx::FontMetrics m{}; f.font->getDefaultMetric(&m);
            f.ascent=m.baseline; f.descent=m.height-m.baseline;
        }
    };
    pick(small_,watchSmallFont(),&fonts::FreeSans12pt7b);
    pick(wide_,vlwFont(),&fonts::lgfxJapanGothic_24);
}
void InfoRowView::end() {
    for (auto& group:groups_) { group.scaled=nullptr; group.maskReady=false; }
}
void InfoRowView::useFont(Gfx& g,const Font& f) const { g.setFont(f.font); g.setTextSize(1); }
void InfoRowView::plan(Gfx& g,const Viewport& viewport,const InfoRow& row,const WatchData& d,bool battery,int items) {
    viewport_=viewport; row_=row;
    items=std::clamp(items,0,MaxItems);
    batteryPercent_=d.batteryPercent>=0 && d.batteryPercent<=100 ? d.batteryPercent : -1;
    charging_=d.chargingKnown && d.charging;
    if (batteryPercent_<0) std::strcpy(battery_,"--%");
    else std::snprintf(battery_,sizeof(battery_),"%d%%",batteryPercent_);
    const int icon=std::min(row.iconSize,MaxIcon);
    // What each group would take whole, then what it may take in this row.
    int natural[InfoRowMaxGroups]{},limits[InfoRowMaxGroups]{},widths[InfoRowMaxGroups]{};
    int n=0;
    if (battery) {
        useFont(g,small_);
        natural[n++]=row.batteryWidth+row.iconGap+g.textWidth(battery_);
    }
    for (int i=0;i<items;++i) {
        groups_[i].wide=!ascii(d.background.items[i].label);
        useFont(g,groups_[i].wide ? wide_ : small_);
        natural[n++]=icon+row.iconGap+g.textWidth(d.background.items[i].label);
    }
    infoGroupLimits(row,natural,n,limits);
    n=0;
    if (battery) { widths[0]=natural[0]; n=1; }
    for (int i=0;i<items;++i) {
        const auto& item=d.background.items[i];
        auto& group=groups_[i];
        useFont(g,group.wide ? wide_ : small_);
        // The app's own text, shortened to fit and otherwise untouched.
        fitText(g,item.label,group.label,sizeof(group.label),std::max(0,limits[n]-icon-row.iconGap));
        widths[n++]=icon+row.iconGap+g.textWidth(group.label);
        // Every icon is drawn in the ink; the suggested colour is not used.
        group.icon=usableIcon(item.icon);
        if (group.icon!=group.scaled) {
            group.scaled=group.icon;
            group.maskReady=group.icon && fitMask(*group.icon,icon,icon,group.mask);
        }
    }
    Rect placed[InfoRowMaxGroups]{};
    placeInfoRow(row,widths,n,placed);
    auto padded=[](Rect r) { return r.empty() ? r : Rect{r.x-Pad,r.y-Pad,r.w+2*Pad,r.h+2*Pad}; };
    boxes_[Battery]=battery ? padded(placed[0]) : Rect{};
    for (int i=0;i<MaxItems;++i) boxes_[Item0+i]=i<items ? padded(placed[i+int(battery)]) : Rect{};
    g.setTextSize(1);
    keys_[Battery]=battery ? hashValue(uint32_t(batteryPercent_+1)|uint32_t(charging_)<<8,hashString(battery_)) : 0u;
    for (int i=0;i<MaxItems;++i) {
        const auto& group=groups_[i];
        keys_[Item0+i]=i<items ? hashValue(uint32_t(reinterpret_cast<uintptr_t>(group.icon))^uint32_t(group.wide),
                                           hashString(group.label,0x51ed27u)) : 0u;
    }
}
void InfoRowView::paintBattery(Gfx& g,uint16_t ink,uint16_t background) const {
    const Rect& box=boxes_[Battery];
    const int x=box.x+Pad,cy=row_.y;
    const int w=row_.batteryWidth,tip=px(viewport_,3),total=row_.batteryHeight;
    const int top=cy-total/2+tip,height=total-tip;
    // Terminal, outline, then the charge rising from the bottom.
    const int tipW=px(viewport_,8);
    g.fillSmoothRoundRect(x+(w-tipW)/2,top-tip,tipW,tip+2,1,ink);
    g.fillSmoothRoundRect(x,top,w,height,3,ink);
    g.fillSmoothRoundRect(x+2,top+2,w-4,height-4,2,background);
    const int inner=height-8;
    if (charging_) {
        const int mx=x+w/2,my=top+height/2;
        drawWideLineClipped(g,mx+2,top+4,mx-2,my,0.9f,ink);
        drawWideLineClipped(g,mx-2,my,mx+2,my,0.9f,ink);
        drawWideLineClipped(g,mx+2,my,mx-2,top+height-5,0.9f,ink);
    } else if (batteryPercent_>=0) {
        const int level=std::max(1,(inner*batteryPercent_+50)/100);
        g.fillRect(x+4,top+4+inner-level,w-8,level,ink);
    }
    useFont(g,small_);
    g.setTextColor(ink,background); g.setTextDatum(baseline_left);
    g.drawString(battery_,x+w+row_.iconGap,cy+(small_.ascent-small_.descent)/2);
}
void InfoRowView::paintItem(Gfx& g,const Group& item,const Rect& box,uint16_t ink,uint16_t background) const {
    const int x=box.x+Pad,cy=row_.y,icon=std::min(row_.iconSize,MaxIcon);
    if (item.maskReady) {
        const int left=x,top=cy-icon/2;
        for (int j=0;j<icon;++j) for (int i=0;i<icon;++i) {
            const uint8_t a=item.mask[j*icon+i];
            if (a) g.drawPixel(left+i,top+j,blend565(ink,background,a));
        }
    } else {
        // No icon from the app: a plain ring stands in.
        g.fillSmoothCircle(x+icon/2,cy,px(viewport_,10),ink);
        g.fillSmoothCircle(x+icon/2,cy,px(viewport_,6),background);
    }
    const Font& f=item.wide ? wide_ : small_;
    useFont(g,f);
    g.setTextColor(ink,background); g.setTextDatum(baseline_left);
    g.drawString(item.label,x+icon+row_.iconGap,cy+(f.ascent-f.descent)/2);
}
void InfoRowView::paint(Gfx& g,int part,uint16_t ink,uint16_t background) const {
    if (part==Battery) paintBattery(g,ink,background);
    else if (part==Item0 || part==Item1) paintItem(g,groups_[part-Item0],boxes_[part],ink,background);
    g.setTextSize(1);
}
}
