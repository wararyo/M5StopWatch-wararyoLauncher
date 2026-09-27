#include "TimeDigits.h"
#include "ui/graphics/WatchFonts.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace launcher {
void TimeDigits::measure(Gfx& g,const VlwGlyphs* glyphs,const lgfx::IFont* fallback,float fallbackSize,int colonLift) {
    glyphs_=glyphs; fallback_=fallback; fallbackSize_=fallbackSize;
    g.setFont(fallback_); g.setTextSize(fallbackSize_);
    int ascent=0,descent=0;
    if (glyphs_) { ascent=glyphs_->ascent(); descent=glyphs_->descent(); }
    else {
        lgfx::FontMetrics m{}; fallback_->getDefaultMetric(&m);
        ascent=int(m.baseline*fallbackSize_); descent=int((m.height-m.baseline)*fallbackSize_);
    }
    // The widest value each group can show, so the colons stay put.
    auto width=[&](const char* s) { return glyphs_ ? glyphs_->width(s) : int(g.textWidth(s)); };
    char two[3];
    int hour=width("--"),minute=hour;
    for (int i=0;i<60;++i) {
        std::snprintf(two,sizeof(two),"%02d",i);
        const int w=width(two);
        if (i<24) hour=std::max(hour,w);
        minute=std::max(minute,w);
    }
    metrics_={ascent,descent,hour,minute,width(":")};
    colonLift_=glyphs_ ? colonLift : 0;
    g.setTextSize(1);
}
int TimeDigits::makeCaches(const TimeGroups& hm,const TimeGroups& hms,bool allowed) {
    int bytes=0;
    auto make=[&](Cache& c,int w,int h) {
        c.ready=false; c.drawn[0]=0;
        if (!allowed) return;
        c.sprite.setPsram(false); c.sprite.setColorDepth(16);
        if (!c.sprite.createSprite(std::max(1,w),std::max(1,h))) return;
        c.ready=true; bytes+=c.sprite.width()*c.sprite.height()*2;
    };
    make(caches_[Hour],hms.hour.w,hms.hour.h);
    make(caches_[Minute],std::max(hms.minute.w,hm.minute.w),hms.minute.h);
    make(caches_[Second],hms.second.w,hms.second.h);
    return bytes;
}
void TimeDigits::release() {
    for (auto& c:caches_) { c.sprite.deleteSprite(); c.ready=false; c.drawn[0]=0; }
}
int TimeDigits::cached() const {
    int n=0;
    for (const auto& c:caches_) n+=int(c.ready);
    return n;
}
void TimeDigits::set(const std::tm& t,bool valid,TimeVariant variant) {
    variant_=variant;
    if (valid) {
        std::snprintf(text_[Hour],sizeof(text_[Hour]),"%02d",t.tm_hour);
        std::snprintf(text_[Minute],sizeof(text_[Minute]),"%02d",t.tm_min);
        std::snprintf(text_[Second],sizeof(text_[Second]),"%02d",std::min(t.tm_sec,59));
    } else for (auto* s:text_) std::strcpy(s,"--");
    if (variant!=TimeVariant::HourMinuteSecond) text_[Second][0]=0;
}
void TimeDigits::drawText(Gfx& g,const char* text,int x,int y,textdatum_t datum,uint16_t fg,uint16_t bg) {
    if (!glyphs_) {
        g.setFont(fallback_); g.setTextSize(fallbackSize_);
        g.setTextColor(fg,bg); g.setTextDatum(datum); g.drawString(text,x,y);
        return;
    }
    const int w=glyphs_->width(text);
    drawGlyphs(g,*glyphs_,text,datum==baseline_right ? x-w : datum==baseline_center ? x-w/2 : x,y,fg,bg);
}
void TimeDigits::drawGroup(Gfx& g,Group group,const TimeGroups& l,int baseline,int dx,int dy,uint16_t fg,uint16_t bg) {
    const int y=baseline-dy;
    switch (group) {
    case Hour:
        drawText(g,text_[Hour],l.hourRight-dx,y,baseline_right,fg,bg);
        drawText(g,":",l.colon1X-dx,y-colonLift_,baseline_left,fg,bg);
        break;
    case Minute:
        if (variant_==TimeVariant::HourMinuteSecond) {
            drawText(g,text_[Minute],l.minuteX-dx,y,baseline_center,fg,bg);
            drawText(g,":",l.colon2X-dx,y-colonLift_,baseline_left,fg,bg);
        } else drawText(g,text_[Minute],l.minuteX-dx,y,baseline_left,fg,bg);
        break;
    default:
        drawText(g,text_[Second],l.secondX-dx,y,baseline_left,fg,bg);
        break;
    }
}
void TimeDigits::paint(Gfx& g,Group group,const TimeGroups& l,int baseline,const Rect& box,uint16_t fg,uint16_t bg) {
    auto& c=caches_[group];
    if (!c.ready) { drawGroup(g,group,l,baseline,0,0,fg,bg); g.setTextSize(1); return; }
    if (std::strcmp(c.drawn,text_[group])!=0 || c.variant!=variant_ || c.fg!=fg || c.bg!=bg) {
        c.sprite.fillScreen(bg);
        drawGroup(c.sprite,group,l,baseline,box.x,box.y,fg,bg);
        static_assert(sizeof(c.drawn)==sizeof(text_[group]),"same size");
        std::memcpy(c.drawn,text_[group],sizeof(c.drawn));
        c.variant=variant_; c.fg=fg; c.bg=bg;
    }
    c.sprite.pushSprite(&g,box.x,box.y);
    g.setTextSize(1);
}
}
