#include "DigitalWatchFace.h"
#include "ui/graphics/MaskImage.h"
#include "ui/graphics/Shapes.h"
#include "ui/graphics/Text.h"
#include "ui/graphics/VlwFont.h"
#include "ui/graphics/WatchFonts.h"
#include <cstdio>
#include <cstring>
namespace launcher {
namespace {
// Colours of docs/Images/WatchFace/Digital*.png (RGB565).
constexpr uint16_t Black=0x0000,White=0xffff,Lime=0xc789,DateGrey=0xcebb,AppsGrey=0x8cf4;
// A chip whose app suggests no colour.
constexpr uint16_t DefaultChip=0xcebb;
constexpr const char* Days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
constexpr const char* Months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
// Black on a light chip, white on a dark one.
uint16_t inkOn(uint16_t fill) {
    const int r=(fill>>11)*255/31,g=((fill>>5)&63)*255/63,b=(fill&31)*255/31;
    return 299*r+587*g+114*b>=150000 ? Black : White;
}
bool ascii(const char* s) {
    for (;*s;++s) if (static_cast<unsigned char>(*s)>=0x80) return false;
    return true;
}
}
void DigitalWatchFace::useFont(Gfx& g,const Font& f) const { g.setFont(f.font); g.setTextSize(f.size); }
void DigitalWatchFace::measure(Gfx& g) {
    auto pick=[&](Font& f,const lgfx::IFont* embedded,const lgfx::IFont* fallback,float size) {
        f.font=embedded ? embedded : fallback; f.size=embedded ? 1 : size;
        useFont(g,f);
        if (f.font->getType()==lgfx::IFont::ft_vlw) {
            const auto* v=static_cast<const lgfx::VLWfont*>(f.font);
            f.ascent=v->maxAscent; f.descent=v->maxDescent;
        } else {
            lgfx::FontMetrics m{}; f.font->getDefaultMetric(&m);
            f.ascent=int(m.baseline*f.size); f.descent=int((m.height-m.baseline)*f.size);
        }
    };
    timeGlyphs_=digitalTimeGlyphs();
    pick(time_,nullptr,&fonts::FreeSansBold24pt7b,2);
    pick(text_,watchTextFont(),&fonts::FreeSans18pt7b,1);
    pick(small_,watchSmallFont(),&fonts::FreeSans12pt7b,1);
    pick(wide_,vlwFont(),&fonts::lgfxJapanGothic_24,1);
    // The widest value each group can show, so the colons stay put.
    useFont(g,time_);
    auto width=[&](const char* s) { return timeGlyphs_ ? timeGlyphs_->width(s) : int(g.textWidth(s)); };
    char two[3];
    int hour=width("--"),minute=hour;
    for (int i=0;i<60;++i) {
        std::snprintf(two,sizeof(two),"%02d",i);
        const int w=width(two);
        if (i<24) hour=std::max(hour,w);
        minute=std::max(minute,w);
    }
    metrics_.timeAscent=timeGlyphs_ ? timeGlyphs_->ascent() : time_.ascent;
    metrics_.timeDescent=timeGlyphs_ ? timeGlyphs_->descent() : time_.descent;
    metrics_.hourWidth=hour; metrics_.minuteWidth=minute; metrics_.colonWidth=width(":");
    // The lift belongs to the embedded digits; a fallback font keeps its own colon.
    metrics_.colonLift=timeGlyphs_ ? DigitalMetrics{}.colonLift : 0;
    metrics_.textAscent=text_.ascent; metrics_.textDescent=text_.descent;
    metrics_.smallAscent=small_.ascent; metrics_.smallDescent=small_.descent;
    g.setTextSize(1);
    auto source=[](const lgfx::IFont* f) { return f && f->getType()==lgfx::IFont::ft_vlw ? "vlw" : "builtin"; };
    std::printf("[WatchFace] digital fonts time=%s text=%s small=%s wide=%s hour=%d minute=%d colon=%d\n",
        timeGlyphs_ ? "vlw" : "builtin",source(text_.font),source(small_.font),source(wide_.font),hour,minute,metrics_.colonWidth);
}
// Sized for the widest the part can be in either variant, once per begin, so
// switching variants reallocates nothing. The time groups live in internal
// RAM for the slide's sake; the chips, drawn less often, in PSRAM. A cache
// that fails is not retried until the next begin: that part is drawn directly.
void DigitalWatchFace::makeCaches() {
    const auto hms=digitalLayout(viewport_,DigitalVariant::HourMinuteSecond,0,metrics_);
    const auto hm=digitalLayout(viewport_,DigitalVariant::HourMinute,0,metrics_);
    int bytes=0;
    auto make=[&](M5Canvas& sprite,int w,int h,bool psram) {
        if (!cacheAllowed_) return false;
        sprite.setPsram(psram); sprite.setColorDepth(16);
        if (!sprite.createSprite(w,h)) return false;
        if (!psram) bytes+=w*h*2;
        return true;
    };
    hourCache_.ready=make(hourCache_.sprite,hms.hour.w,hms.hour.h,false);
    minuteCache_.ready=make(minuteCache_.sprite,std::max(hms.minute.w,hm.minute.w),hms.minute.h,false);
    secondCache_.ready=make(secondCache_.sprite,hms.second.w,hms.second.h,false);
    int psram=0;
    for (auto& p:chips_) {
        p.cacheReady=make(p.sprite,std::max(1,hms.chipsWidth),std::max(1,hms.chipHeight),true);
        if (p.cacheReady) psram+=p.sprite.width()*p.sprite.height()*2;
    }
    std::printf("[WatchFace] digital cache time=%d/3 internal_bytes=%d items=%d/2 psram_bytes=%d\n",
        int(hourCache_.ready)+int(minuteCache_.ready)+int(secondCache_.ready),bytes,
        int(chips_[0].cacheReady)+int(chips_[1].cacheReady),psram);
}
int DigitalWatchFace::cachedParts() const {
    return int(hourCache_.ready)+int(minuteCache_.ready)+int(secondCache_.ready)+
           int(chips_[0].cacheReady)+int(chips_[1].cacheReady);
}
bool DigitalWatchFace::begin(Gfx& g,bool disableCache) {
    end();
    viewport_={int(g.width()),int(g.height())};
    cacheAllowed_=!disableCache;
    measure(g);
    makeCaches();
    return true;
}
void DigitalWatchFace::end() {
    // The variant is the face's setting, not part of its caches: it stays.
    for (auto* c:{&hourCache_,&minuteCache_,&secondCache_}) { c->sprite.deleteSprite(); c->ready=false; c->drawn[0]=0; }
    for (auto& p:chips_) {
        p.sprite.deleteSprite(); p.cacheReady=false; p.drawnKey=0; p.scaled=nullptr; p.maskReady=false;
    }
    elements_={};
}
void DigitalWatchFace::plan(FramePlan& frame,Gfx& g,const WatchEnvironment& env,const WatchData& d) {
    // The whole face rides up with the list's edge, as it always has.
    viewport_=env.viewport; clip_=env.clip;
    offset_=-static_cast<int>(env.listProgress*viewport_.height);
    variant_=control_.variant();
    const bool seconds=variant_==DigitalVariant::HourMinuteSecond;
    const auto& t=d.localTime;
    const bool valid=d.timeValid && t.tm_hour>=0 && t.tm_hour<24 && t.tm_min>=0 && t.tm_min<60 &&
        t.tm_sec>=0 && t.tm_sec<=60 && t.tm_wday>=0 && t.tm_wday<7 && t.tm_mon>=0 && t.tm_mon<12 &&
        t.tm_mday>=1 && t.tm_mday<=31;
    if (valid) {
        std::snprintf(hour_,sizeof(hour_),"%02d",t.tm_hour);
        std::snprintf(minute_,sizeof(minute_),"%02d",t.tm_min);
        // A leap second shows as :59 rather than widening the digits.
        std::snprintf(second_,sizeof(second_),"%02d",std::min(t.tm_sec,59));
        std::snprintf(date_,sizeof(date_),"%s, %s %02d",Days[t.tm_wday],Months[t.tm_mon],t.tm_mday);
    } else {
        std::strcpy(hour_,"--"); std::strcpy(minute_,"--"); std::strcpy(second_,"--");
        std::strcpy(date_,"SET TIME");
    }
    if (!seconds) second_[0]=0;
    batteryPercent_=d.batteryPercent>=0 && d.batteryPercent<=100 ? d.batteryPercent : -1;
    charging_=d.charging;
    if (batteryPercent_<0) std::strcpy(battery_,"--%");
    else std::snprintf(battery_,sizeof(battery_),"%d%%",batteryPercent_);

    chipCount_=std::min<int>(d.background.count,DigitalMaxItems);
    layout_=digitalLayout(viewport_,variant_,chipCount_,metrics_);
    // Each chip: the icon on its left cap, the label after it. The label is
    // the app's own text, shortened to fit and otherwise untouched.
    const int h=layout_.chipHeight,pad=digital::px(viewport_,16);
    const int limit=digitalChipWidthLimit(layout_,chipCount_);
    int widths[DigitalMaxItems]{};
    for (int i=0;i<chipCount_;++i) {
        const auto& item=d.background.items[i];
        auto& p=chips_[i];
        p.wide=!ascii(item.label);
        useFont(g,p.wide ? wide_ : small_);
        fitText(g,item.label,p.label,sizeof(p.label),std::max(0,limit-h-pad));
        widths[i]=h+g.textWidth(p.label)+pad;
        p.icon=usableIcon(item.icon);
        p.fill=item.suggestedColor ? *item.suggestedColor : DefaultChip;
        p.ink=inkOn(p.fill);
        if (p.icon!=p.scaled) {
            p.scaled=p.icon;
            p.maskReady=p.icon && fitMask(*p.icon,IconSize,IconSize,p.mask);
        }
        uint32_t key=hashString(p.label);
        key=hashValue(uint32_t(reinterpret_cast<uintptr_t>(p.icon)),key);
        key=hashValue(p.fill|uint32_t(p.wide)<<16,key);
        p.key=hashValue(uint32_t(widths[i]),key);
    }
    Rect chipBoxes[DigitalMaxItems]{};
    placeDigitalChips(layout_,widths,chipCount_,chipBoxes);
    for (int i=0;i<DigitalMaxItems;++i) chips_[i].box=i<chipCount_ ? chipBoxes[i] : Rect{};

    // Battery row: the icon, then the percent, centred together.
    useFont(g,small_);
    const int icon=digital::px(viewport_,26),gap=digital::px(viewport_,9);
    const int row=icon+gap+g.textWidth(battery_),rowLeft=layout_.cx-row/2;
    const int baseline=layout_.batteryY+(small_.ascent-small_.descent)/2;
    const int top=std::min(layout_.batteryY-digital::px(viewport_,7),baseline-small_.ascent);
    const int bottom=std::max(layout_.batteryY+digital::px(viewport_,7),baseline+small_.descent);
    boxes_[Battery]={rowLeft-2,top-2,row+4,bottom-top+4};
    useFont(g,text_);
    const int dateWidth=g.textWidth(date_);
    boxes_[Date]={layout_.cx-dateWidth/2-2,layout_.dateBaseline-text_.ascent-2,dateWidth+4,text_.ascent+text_.descent+4};
    boxes_[Hour]=layout_.hour; boxes_[Minute]=layout_.minute; boxes_[Second]=layout_.second;
    boxes_[Item0]=chips_[0].box; boxes_[Item1]=chips_[1].box;
    useFont(g,small_);
    const int labelWidth=g.textWidth("APPS");
    const Rect label={layout_.cx-labelWidth/2-2,layout_.appsBaseline-small_.ascent-2,labelWidth+4,small_.ascent+small_.descent+4};
    boxes_[Apps]=unite(layout_.appsIcon,label);
    g.setTextSize(1);

    const uint32_t hashes[PartCount]={
        hashValue(uint32_t(batteryPercent_+1)|uint32_t(charging_)<<8,hashString(battery_)),
        hashString(date_),hashString(hour_),hashValue(uint32_t(variant_),hashString(minute_)),hashString(second_),
        chips_[0].key,chips_[1].key,0xa995};
    for (int i=0;i<PartCount;++i) {
        boxes_[i]=boxes_[i].empty() ? Rect{} : shifted(boxes_[i]);
        frame.add(elements_[i],intersect(boxes_[i],clip_),hashes[i]);
    }
}
void DigitalWatchFace::paintBattery(Gfx& g,int dx,int dy) {
    const auto& b=boxes_[Battery];
    const int icon=digital::px(viewport_,26),gap=digital::px(viewport_,9);
    const int x=b.x+2-dx,cy=layout_.batteryY+offset_-dy;
    const int body=icon-digital::px(viewport_,3),h=digital::px(viewport_,14),top=cy-h/2;
    // Outline, the terminal, then the charge inside.
    g.fillSmoothRoundRect(x,top,body,h,3,Lime);
    g.fillSmoothRoundRect(x+2,top+2,body-4,h-4,2,Black);
    g.fillSmoothRoundRect(x+body+1,top+h/2-3,icon-body-1,6,1,Lime);
    const int inner=body-8;
    if (charging_) {
        // A bolt across the empty body.
        const int cx=x+body/2;
        drawWideLineClipped(g,cx+2,top+3,cx-2,top+h/2,0.9f,Lime);
        drawWideLineClipped(g,cx-2,top+h/2,cx+2,top+h/2,0.9f,Lime);
        drawWideLineClipped(g,cx+2,top+h/2,cx-2,top+h-3,0.9f,Lime);
    } else if (batteryPercent_>=0) {
        const int level=std::max(1,(inner*batteryPercent_+50)/100);
        g.fillRect(x+4,top+4,level,h-8,Lime);
    }
    useFont(g,small_);
    g.setTextColor(Lime,Black); g.setTextDatum(baseline_left);
    g.drawString(battery_,x+icon+gap,cy+(small_.ascent-small_.descent)/2);
}
void DigitalWatchFace::paintDate(Gfx& g,int dx,int dy) {
    useFont(g,text_);
    g.setTextColor(DateGrey,Black); g.setTextDatum(baseline_center);
    g.drawString(date_,layout_.cx-dx,layout_.dateBaseline+offset_-dy);
}
void DigitalWatchFace::drawTime(Gfx& g,const char* text,int x,int y,textdatum_t datum) {
    if (!timeGlyphs_) {
        useFont(g,time_); g.setTextColor(White,Black); g.setTextDatum(datum); g.drawString(text,x,y);
        return;
    }
    const int w=timeGlyphs_->width(text);
    drawGlyphs(g,*timeGlyphs_,text,datum==baseline_right ? x-w : datum==baseline_center ? x-w/2 : x,y,White,Black);
}
void DigitalWatchFace::paintHour(Gfx& g,int dx,int dy) {
    const int y=layout_.timeBaseline+offset_-dy;
    drawTime(g,hour_,layout_.hourRight-dx,y,baseline_right);
    drawTime(g,":",layout_.colon1X-dx,y-metrics_.colonLift,baseline_left);
}
void DigitalWatchFace::paintMinute(Gfx& g,int dx,int dy) {
    const int y=layout_.timeBaseline+offset_-dy;
    if (variant_==DigitalVariant::HourMinuteSecond) {
        drawTime(g,minute_,layout_.minuteX-dx,y,baseline_center);
        drawTime(g,":",layout_.colon2X-dx,y-metrics_.colonLift,baseline_left);
    } else drawTime(g,minute_,layout_.minuteX-dx,y,baseline_left);
}
void DigitalWatchFace::paintSecond(Gfx& g,int dx,int dy) {
    drawTime(g,second_,layout_.secondX-dx,layout_.timeBaseline+offset_-dy,baseline_left);
}
void DigitalWatchFace::paintChip(Gfx& g,const Chip& p,int dx,int dy) {
    const Rect b=shifted(p.box);
    const int x=b.x-dx,y=b.y-dy,h=b.h;
    g.fillSmoothRoundRect(x,y,b.w,h,h/2,p.fill);
    // The icon sits on the left cap. Its mask is blended straight into the
    // chip, so its corners never paint chip colour outside the rounding.
    const int cx=x+h/2,cy=y+h/2;
    if (p.maskReady) {
        const int left=cx-IconSize/2,top=cy-IconSize/2;
        for (int j=0;j<IconSize;++j) for (int i=0;i<IconSize;++i) {
            const uint8_t a=p.mask[j*IconSize+i];
            if (a) g.drawPixel(left+i,top+j,blend565(p.ink,p.fill,a));
        }
    } else {
        // No icon from the app: a plain ring stands in.
        g.fillSmoothCircle(cx,cy,digital::px(viewport_,10),p.ink);
        g.fillSmoothCircle(cx,cy,digital::px(viewport_,6),p.fill);
    }
    const Font& f=p.wide ? wide_ : small_;
    useFont(g,f);
    g.setTextColor(p.ink,p.fill); g.setTextDatum(baseline_left);
    // One pixel below the centre of the line: it reads as centred on the chip.
    g.drawString(p.label,x+h,cy+(f.ascent-f.descent)/2+1);
}
void DigitalWatchFace::paintApps(Gfx& g,int dx,int dy) {
    const Rect icon=shifted(layout_.appsIcon);
    const int x=icon.x-dx,y=icon.y-dy,size=digital::px(viewport_,6),pitch=icon.w-size;
    for (int i=0;i<2;++i) for (int j=0;j<2;++j) g.fillRect(x+i*pitch,y+j*pitch,size,size,Lime);
    useFont(g,small_);
    g.setTextColor(AppsGrey,Black); g.setTextDatum(baseline_center);
    g.drawString("APPS",layout_.cx-dx,layout_.appsBaseline+offset_-dy);
}
void DigitalWatchFace::paintTime(Gfx& g,TimeCache& cache,const char* text,const Rect& box,
                                 void (DigitalWatchFace::*draw)(Gfx&,int,int)) {
    if (!cache.ready) { (this->*draw)(g,0,0); return; }
    if (std::strcmp(cache.drawn,text)!=0 || cache.variant!=variant_) {
        cache.sprite.fillScreen(Black);
        (this->*draw)(cache.sprite,box.x,box.y);
        std::snprintf(cache.drawn,sizeof(cache.drawn),"%s",text);
        cache.variant=variant_;
    }
    cache.sprite.pushSprite(&g,box.x,box.y);
}
void DigitalWatchFace::paint(Gfx& g,const PaintContext& context) {
    for (int i=0;i<PartCount;++i) {
        if (!context.clip(g,boxes_[i])) continue;
        switch (i) {
        case Battery: paintBattery(g,0,0); break;
        case Date: paintDate(g,0,0); break;
        case Hour: paintTime(g,hourCache_,hour_,boxes_[Hour],&DigitalWatchFace::paintHour); break;
        case Minute: paintTime(g,minuteCache_,minute_,boxes_[Minute],&DigitalWatchFace::paintMinute); break;
        case Second: paintTime(g,secondCache_,second_,boxes_[Second],&DigitalWatchFace::paintSecond); break;
        case Item0: case Item1: {
            auto& p=chips_[i-Item0];
            if (!p.cacheReady) { paintChip(g,p,0,0); break; }
            const Rect b=boxes_[i];
            if (p.drawnKey!=p.key) {
                p.sprite.fillScreen(Black);
                paintChip(p.sprite,p,b.x,b.y);
                p.drawnKey=p.key;
            }
            p.sprite.pushSprite(&g,b.x,b.y);
            break;
        }
        case Apps: paintApps(g,0,0); break;
        }
    }
    g.setTextSize(1);
}
}
