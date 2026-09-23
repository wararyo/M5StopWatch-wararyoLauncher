#include "DigitalWatchFace.h"
#include "ui/rendering/Scale.h"
#include <cstdio>
#include <cstring>
namespace launcher {
namespace {
constexpr uint16_t White=0xf7be,Muted=0xad75,Lime=0xb7e0;
constexpr const char* Days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
constexpr const char* Months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
Rect textBox(Gfx& g,const char* text,int x,int y) {
    const int w=g.textWidth(text)+12,h=g.fontHeight()+12;
    return {x-w/2,y-h/2,w,h};
}
}
void DigitalWatchFace::timeFont(Gfx& g) { g.setFont(&fonts::FreeSansBold24pt7b); g.setTextSize(2.0f*scale_); }
bool DigitalWatchFace::begin(Gfx& g,bool disableCache) {
    end();
    scale_=float(std::min(g.width(),g.height()))/468;
    timeFont(g);
    const int w=g.textWidth("00:00")+12,h=g.fontHeight()+12;
    cache_.setPsram(false); cache_.setColorDepth(16);
    cacheReady_=!disableCache && cache_.createSprite(w,h)!=nullptr;
    g.setTextSize(1);
    std::printf("[WatchFace] digital cache=%s bytes=%d internal\n",cacheReady_ ? "ready" : "direct",w*h*2);
    return true;
}
void DigitalWatchFace::end() {
    cache_.deleteSprite(); cacheReady_=false; cached_[0]=0; elements_={};
}
void DigitalWatchFace::plan(FramePlan& frame,Gfx& g,const DrawRegion& region,const WatchData& d) {
    const auto& m=region.viewport;
    viewport_=m; cx_=m.width/2; offset_=region.offsetY;
    clip_=region.clip;
    const bool valid=d.timeValid && d.localTime.tm_hour>=0 && d.localTime.tm_hour<24 &&
        d.localTime.tm_min>=0 && d.localTime.tm_min<60 && d.localTime.tm_wday>=0 &&
        d.localTime.tm_wday<7 && d.localTime.tm_mon>=0 && d.localTime.tm_mon<12 &&
        d.localTime.tm_mday>=1 && d.localTime.tm_mday<=31;
    if(valid) {
        std::snprintf(time_,sizeof(time_),"%02d:%02d",d.localTime.tm_hour,d.localTime.tm_min);
        std::snprintf(date_,sizeof(date_),"%s, %s %02d",Days[d.localTime.tm_wday],Months[d.localTime.tm_mon],d.localTime.tm_mday);
    } else { std::snprintf(time_,sizeof(time_),"--:--"); std::snprintf(date_,sizeof(date_),"SET TIME"); }
    if(d.batteryPercent<0 || d.batteryPercent>100) std::snprintf(battery_,sizeof(battery_),"%s--%%",d.charging ? "+ " : "");
    else std::snprintf(battery_,sizeof(battery_),"%s%d%%",d.charging ? "+ " : "",d.batteryPercent);
    g.setTextSize(scale_); g.setFont(&fonts::FreeSans18pt7b);
    boxes_[0]=textBox(g,battery_,cx_,offset_+scaled(m,80));
    boxes_[1]=textBox(g,date_,cx_,offset_+scaled(m,123));
    timeFont(g);
    timeBox_=textBox(g,time_,cx_,offset_+scaled(m,250));
    if(cacheReady_) timeBox_={cx_-cache_.width()/2,offset_+scaled(m,250)-cache_.height()/2,cache_.width(),cache_.height()};
    boxes_[2]=timeBox_;
    boxes_[3]={cx_-scaled(m,16),offset_+scaled(m,357),scaled(m,32),scaled(m,32)};
    g.setTextSize(scale_); g.setFont(&fonts::FreeSans12pt7b);
    boxes_[4]=textBox(g,"APPS",cx_,offset_+scaled(m,400));
    const uint32_t hashes[]={hashString(battery_),hashString(date_),hashString(time_),0xd075,0xa995};
    for(int i=0;i<5;++i) handles_[i]=frame.add(elements_[i],intersect(boxes_[i],clip_),hashes[i]);
}
void DigitalWatchFace::paintTime(Gfx& g) {
    if(cacheReady_) {
        if(std::strcmp(time_,cached_)!=0) {
            std::snprintf(cached_,sizeof(cached_),"%s",time_);
            cache_.fillScreen(0); cache_.setTextDatum(middle_center); cache_.setTextColor(White,0);
            timeFont(cache_); cache_.drawString(time_,cache_.width()/2,cache_.height()/2);
        }
        cache_.pushSprite(&g,timeBox_.x,timeBox_.y);
    } else {
        timeFont(g); g.setTextColor(White,0); g.setTextDatum(middle_center);
        g.drawString(time_,timeBox_.x+timeBox_.w/2,timeBox_.y+timeBox_.h/2);
    }
}
void DigitalWatchFace::paint(Gfx& g,const FramePlan& frame) {
    if(clip_.empty()) return;
    for(int i=0;i<5;++i) {
        Rect clip=intersect(boxes_[i],clip_);
        if(clip.empty() || !frame.shouldPaint(handles_[i])) continue;
        g.setClipRect(clip.x,clip.y,clip.w,clip.h);
        g.setTextDatum(middle_center); g.setTextSize(scale_);
        if(i==2) paintTime(g);
        else if(i==3) {
            const int size=scaled(viewport_,10),gap=scaled(viewport_,20);
            for(int x=0;x<2;++x) for(int y=0;y<2;++y)
                g.fillRect(boxes_[i].x+x*gap,boxes_[i].y+y*gap,size,size,Lime);
        } else {
            g.setFont(i==4 ? &fonts::FreeSans12pt7b : &fonts::FreeSans18pt7b);
            g.setTextColor(i==0 ? Lime : Muted,0);
            g.drawString(i==0 ? battery_ : i==1 ? date_ : "APPS",cx_,boxes_[i].y+boxes_[i].h/2);
        }
    }
    g.clearClipRect(); g.setTextSize(1);
}
}
