#include "ForestWatchFace.h"
#include "features/background/BackgroundInfoHub.h"
#include "ui/graphics/MaskImage.h"
#include "ui/graphics/Shapes.h"
#include "ui/graphics/Text.h"
#include "ui/graphics/VlwFont.h"
#include "ui/graphics/WatchFonts.h"
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
}
bool ForestWatchFace::begin(Gfx& g,bool disableCache) {
    end();
    viewport_={int(g.width()),int(g.height())};
    cacheAllowed_=!disableCache;
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
    digits_.measure(g,forestTimeGlyphs(),&fonts::FreeSansBold24pt7b,2,ForestMetrics{}.colonLift);
    const auto& t=digits_.metrics();
    metrics_.timeAscent=t.ascent; metrics_.timeDescent=t.descent;
    metrics_.hourWidth=t.hourWidth; metrics_.minuteWidth=t.minuteWidth; metrics_.colonWidth=t.colonWidth;
    metrics_.colonLift=digits_.colonLift();
    metrics_.smallAscent=small_.ascent; metrics_.smallDescent=small_.descent;
    // The groups have the same size in both layouts; only their place moves.
    const int bytes=digits_.makeCaches(forestLayout(viewport_,TimeVariant::HourMinute,false,metrics_).time,
                                       forestLayout(viewport_,TimeVariant::HourMinuteSecond,false,metrics_).time,cacheAllowed_);
    std::printf("[WatchFace] forest fonts time=%s hour=%d minute=%d colon=%d cache time=%d/3 internal_bytes=%d\n",
        digits_.embedded() ? "vlw" : "builtin",t.hourWidth,t.minuteWidth,t.colonWidth,digits_.cached(),bytes);
    g.setTextSize(1);
    return true;
}
void ForestWatchFace::end() {
    // The variant and the battery rule's state are the face's, not its
    // caches': they stay.
    digits_.release();
    for (auto& group:groups_) { group.scaled=nullptr; group.maskReady=false; }
    elements_={};
    planned_=false;
}
void ForestWatchFace::plan(FramePlan& frame,Gfx& g,const WatchEnvironment& env,const WatchData& d) {
    viewport_=env.viewport; clip_=env.clip;
    const bool info=control_.info();
    layout_=forestLayout(viewport_,control_.variant(),info,metrics_);
    // The scenery is background, not an element: when it moves, all of it
    // that shows changes; when the list's edge moves, the band between the
    // two edges does (docs/task10/plan-10-4.md 1).
    if (planned_ && info!=shownInfo_) frame.damage(unite(clip_,shownClip_));
    else if (planned_ && clip_!=shownClip_) {
        const int a=shownClip_.y+shownClip_.h,b=clip_.y+clip_.h;
        frame.damage({0,std::min(a,b),viewport_.width,std::abs(a-b)});
    }
    planned_=true; shownInfo_=info; shownClip_=clip_;

    const auto& t=d.localTime;
    const bool valid=d.timeValid && t.tm_hour>=0 && t.tm_hour<24 && t.tm_min>=0 && t.tm_min<60 &&
        t.tm_sec>=0 && t.tm_sec<=60;
    digits_.set(t,valid,control_.variant());
    boxes_[Hour]=layout_.time.hour; boxes_[Minute]=layout_.time.minute;
    boxes_[Second]=control_.variant()==TimeVariant::HourMinuteSecond ? layout_.time.second : Rect{};

    // The information row: the battery when it shows, then the items.
    batteryShown_=control_.batteryShown();
    batteryPercent_=d.batteryPercent>=0 && d.batteryPercent<=100 ? d.batteryPercent : -1;
    charging_=d.chargingKnown && d.charging;
    if (batteryPercent_<0) std::strcpy(battery_,"--%");
    else std::snprintf(battery_,sizeof(battery_),"%d%%",batteryPercent_);
    const int items=control_.items();
    const int icon=std::min(layout_.row.iconSize,MaxIcon);
    // What each group would take whole, then what it may take in this row.
    int natural[ForestMaxItems+1]{},limits[ForestMaxItems+1]{},widths[ForestMaxItems+1]{};
    int n=0;
    if (batteryShown_) {
        useFont(g,small_);
        natural[n++]=layout_.row.batteryWidth+layout_.row.iconGap+g.textWidth(battery_);
    }
    for (int i=0;i<items;++i) {
        groups_[i].wide=!ascii(d.background.items[i].label);
        useFont(g,groups_[i].wide ? wide_ : small_);
        natural[n++]=icon+layout_.row.iconGap+g.textWidth(d.background.items[i].label);
    }
    infoGroupLimits(layout_.row,natural,n,limits);
    n=0;
    if (batteryShown_) { widths[0]=natural[0]; n=1; }
    for (int i=0;i<items;++i) {
        const auto& item=d.background.items[i];
        auto& group=groups_[i];
        useFont(g,group.wide ? wide_ : small_);
        // The app's own text, shortened to fit and otherwise untouched.
        fitText(g,item.label,group.label,sizeof(group.label),std::max(0,limits[n]-icon-layout_.row.iconGap));
        widths[n++]=icon+layout_.row.iconGap+g.textWidth(group.label);
        // Forest draws every icon white and ignores the suggested colour.
        group.icon=usableIcon(item.icon);
        if (group.icon!=group.scaled) {
            group.scaled=group.icon;
            group.maskReady=group.icon && fitMask(*group.icon,icon,icon,group.mask);
        }
    }
    Rect placed[ForestMaxItems+1]{};
    placeInfoRow(layout_.row,widths,n,placed);
    auto padded=[](Rect r) { return r.empty() ? r : Rect{r.x-Pad,r.y-Pad,r.w+2*Pad,r.h+2*Pad}; };
    boxes_[Battery]=batteryShown_ ? padded(placed[0]) : Rect{};
    for (int i=0;i<ForestMaxItems;++i)
        boxes_[Item0+i]=i<items ? padded(placed[i+int(batteryShown_)]) : Rect{};
    g.setTextSize(1);

    auto itemHash=[&](int i) -> uint32_t {
        if (i>=items) return 0u;
        const auto& group=groups_[i];
        uint32_t h=hashString(group.label,0x51ed27u);
        return hashValue(uint32_t(reinterpret_cast<uintptr_t>(group.icon))^uint32_t(group.wide),h);
    };
    const uint32_t hashes[PartCount]={
        hashString(digits_.text(TimeDigits::Hour)),
        hashValue(uint32_t(control_.variant()),hashString(digits_.text(TimeDigits::Minute))),
        hashString(digits_.text(TimeDigits::Second)),
        hashValue(uint32_t(batteryPercent_+1)|uint32_t(charging_)<<8,hashString(battery_)),
        itemHash(0),itemHash(1)};
    for (int i=0;i<PartCount;++i) frame.add(elements_[i],intersect(boxes_[i],clip_),hashes[i]);
}
void ForestWatchFace::paintScenery(Gfx& g,const PaintContext& context) {
    if (!context.clip(g,clip_)) return;
    const Rect area=context.area();
    const int w=viewport_.width,h=viewport_.height,ground=layout_.groundTop;
    g.fillRect(0,0,w,ground,ForestSky);
    g.fillRect(0,ground,w,h-ground,ForestGround);
    for (const auto& t:layout_.trees) {
        const Rect bounds{int(t.lx)-2,int(t.ay)-2,int(t.rx-t.lx)+5,int(t.by-t.ay)+5};
        if (bounds.intersects(area)) fillSmoothTriangleClipped(g,t.ax,t.ay,t.lx,t.by,t.rx,t.by,ForestTree);
    }
}
void ForestWatchFace::paintBattery(Gfx& g,const Rect& box) {
    const int x=box.x+Pad,cy=layout_.row.y;
    const int w=layout_.row.batteryWidth,tip=forest::px(viewport_,3),total=layout_.row.batteryHeight;
    const int top=cy-total/2+tip,height=total-tip;
    // Terminal, outline, then the charge rising from the bottom.
    const int tipW=forest::px(viewport_,8);
    g.fillSmoothRoundRect(x+(w-tipW)/2,top-tip,tipW,tip+2,1,ForestInk);
    g.fillSmoothRoundRect(x,top,w,height,3,ForestInk);
    g.fillSmoothRoundRect(x+2,top+2,w-4,height-4,2,ForestGround);
    const int inner=height-8;
    if (charging_) {
        const int mx=x+w/2,my=top+height/2;
        drawWideLineClipped(g,mx+2,top+4,mx-2,my,0.9f,ForestInk);
        drawWideLineClipped(g,mx-2,my,mx+2,my,0.9f,ForestInk);
        drawWideLineClipped(g,mx+2,my,mx-2,top+height-5,0.9f,ForestInk);
    } else if (batteryPercent_>=0) {
        const int level=std::max(1,(inner*batteryPercent_+50)/100);
        g.fillRect(x+4,top+4+inner-level,w-8,level,ForestInk);
    }
    useFont(g,small_);
    g.setTextColor(ForestInk,ForestGround); g.setTextDatum(baseline_left);
    g.drawString(battery_,x+w+layout_.row.iconGap,cy+(small_.ascent-small_.descent)/2);
}
void ForestWatchFace::paintItem(Gfx& g,const Group& item,const Rect& box) {
    const int x=box.x+Pad,cy=layout_.row.y,icon=std::min(layout_.row.iconSize,MaxIcon);
    if (item.maskReady) {
        const int left=x,top=cy-icon/2;
        for (int j=0;j<icon;++j) for (int i=0;i<icon;++i) {
            const uint8_t a=item.mask[j*icon+i];
            if (a) g.drawPixel(left+i,top+j,blend565(ForestInk,ForestGround,a));
        }
    } else {
        // No icon from the app: a plain ring stands in.
        g.fillSmoothCircle(x+icon/2,cy,forest::px(viewport_,10),ForestInk);
        g.fillSmoothCircle(x+icon/2,cy,forest::px(viewport_,6),ForestGround);
    }
    const Font& f=item.wide ? wide_ : small_;
    useFont(g,f);
    g.setTextColor(ForestInk,ForestGround); g.setTextDatum(baseline_left);
    g.drawString(item.label,x+icon+layout_.row.iconGap,cy+(f.ascent-f.descent)/2);
}
void ForestWatchFace::paint(Gfx& g,const PaintContext& context) {
    paintScenery(g,context);
    for (int i=0;i<PartCount;++i) {
        if (!context.clip(g,boxes_[i])) continue;
        switch (i) {
        case Hour: digits_.paint(g,TimeDigits::Hour,layout_.time,layout_.timeBaseline,boxes_[i],ForestInk,ForestSky); break;
        case Minute: digits_.paint(g,TimeDigits::Minute,layout_.time,layout_.timeBaseline,boxes_[i],ForestInk,ForestSky); break;
        case Second: digits_.paint(g,TimeDigits::Second,layout_.time,layout_.timeBaseline,boxes_[i],ForestInk,ForestSky); break;
        case Battery: paintBattery(g,boxes_[i]); break;
        default: paintItem(g,groups_[i-Item0],boxes_[i]); break;
        }
    }
    g.setTextSize(1);
}
}
