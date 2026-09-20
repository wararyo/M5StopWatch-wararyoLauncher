#include "Renderer.h"
#include "IconSet.h"
#include "Text.h"
#include "VlwFont.h"
#include "app/AppRegistry.h"
#include <cstdio>
#include <cstring>
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "RenderDiagnostics.h"
#include <esp_timer.h>
#endif
namespace launcher {
namespace {
constexpr uint16_t White=0xf7be,Muted=0xad75,Lime=0xb7e0;
constexpr uint16_t Colors[]={0x349f,0x632c,0x2e17,0x2e17,0x2e17};
}
bool Renderer::registerFace(WatchFace& face) {
    for(auto* registered:registry_) if(registered && std::strcmp(registered->id(),face.id())==0) return false;
    for(auto& slot:registry_) if(!slot) { slot=&face; return true; }
    return false;
}
bool Renderer::selectFace(const char* id,bool disableCache) {
    for(auto* candidate:registry_) if(candidate && std::strcmp(candidate->id(),id)==0) {
        WatchFace* previous=face_;
        if(previous) previous->end();
        if(!candidate->begin(display_,disableCache)) {
            candidate->end();
            face_=previous && previous->begin(display_,true) ? previous : nullptr;
            invalidate(); return false;
        }
        face_=candidate; invalidate(); return true;
    }
    return false;
}
bool Renderer::begin(bool disableCache) {
    // The list keeps working on the built-in font if the embedded subset fails.
    if(const auto* embedded=vlwFont()) nameFont_=embedded;
    registerFace(digital_);
    return display_.width()>0 && display_.height()>0 && selectFace("digital",disableCache);
}
void Renderer::planList(const ScreenModel& m) {
    const float scale=float(std::min(m.width,m.height))/468;
    for(int i=0;i<5;++i) {
        auto& row=plannedRows_[i]; row.layout=layoutRow(m,i);
        const auto& box=row.layout.box;
        if(box.empty()) {
            row.name[0]=0;
            row.handle=frame_.add(rows_[i],{},0);
            continue;
        }
        display_.setFont(nameFont_); display_.setTextSize(scale);
        fitText(display_,m.names[i] ? m.names[i] : AppRegistry[i].name,row.name,sizeof(row.name),std::max(0,box.x+box.w-row.layout.labelX-4));
        uint32_t hash=hashValue(i==m.selection,hashString(row.name));
        hash=hashValue(row.layout.centerY,hashValue(row.layout.iconX,hash));
        row.handle=frame_.add(rows_[i],box,hash);
    }
    const int offset=int((1-m.transition)*m.height);
    hintBox_=intersect({m.width/2-scaled(m,72),offset+scaled(m,433),scaled(m,144),scaled(m,20)},{0,0,m.width,m.height});
    hintHandle_=frame_.add(hint_,hintBox_,0x1234);
    toastBox_=m.toast ? Rect{m.width/2-scaled(m,108),scaled(m,361),scaled(m,216),scaled(m,46)} : Rect{};
    toastHandle_=frame_.add(toast_,toastBox_,m.toast ? hashString(m.toast) : 0);
}
void Renderer::paintList(const ScreenModel& m) {
    const float scale=float(std::min(m.width,m.height))/468;
    for(int i=0;i<5;++i) {
        const auto& row=plannedRows_[i]; const auto& r=row.layout; const auto& b=r.box;
        if(b.empty() || !frame_.shouldPaint(row.handle)) continue;
        display_.setClipRect(b.x,b.y,b.w,b.h);
        const int radius=r.radius-(i==m.selection ? 0 : selectionGrowth(m));
        // An even diameter, so the circle centres on the pixel boundary at
        // (iconX, centerY) where the 44px mask and the even height text box
        // centre too. fillCircle would cover 2r+1 and land half a pixel off.
        display_.fillSmoothRoundRect(r.iconX-radius,r.centerY-radius,2*radius,2*radius,radius,Colors[i]);
        // The mask fits inside the circle, so its zero pixels restore the
        // circle colour and no pixel outside the circle is touched.
        if(const auto* icon=appIcon(AppRegistry[i].icon)) {
            if(scale==1.0f)
                display_.pushGrayscaleImage(r.iconX-icon->width/2,r.centerY-icon->height/2,
                    icon->width,icon->height,icon->pixels,lgfx::grayscale_8bit,White,Colors[i]);
            else
                // Rotate-zoom takes pixel indices and adds half a pixel to each,
                // so both centres are given as index-0.5 to stay on the boundary.
                display_.pushGrayscaleImageRotateZoom(r.iconX-0.5f,r.centerY-0.5f,
                    icon->width*0.5f-0.5f,icon->height*0.5f-0.5f,0.0f,scale,scale,
                    icon->width,icon->height,icon->pixels,lgfx::grayscale_8bit,White,Colors[i]);
        }
        display_.setFont(nameFont_); display_.setTextSize(scale);
        display_.setTextDatum(middle_left); display_.setTextColor(i==m.selection ? Lime : White,0);
        display_.drawString(row.name,r.labelX,r.centerY);
    }
    if(!hintBox_.empty() && frame_.shouldPaint(hintHandle_)) {
        display_.setClipRect(hintBox_.x,hintBox_.y,hintBox_.w,hintBox_.h);
        display_.setTextDatum(middle_center); display_.setTextSize(scale);
        display_.setFont(&fonts::Font0); display_.setTextColor(Muted,0);
        display_.drawString("A NEXT  B OK  A+B HOME",hintBox_.x+hintBox_.w/2,hintBox_.y+scaled(m,10));
    }
    if(!toastBox_.empty() && frame_.shouldPaint(toastHandle_)) {
        const auto& b=toastBox_; display_.setClipRect(b.x,b.y,b.w,b.h);
        display_.fillRoundRect(b.x,b.y,b.w,b.h,scaled(m,14),0x2104);
        display_.setFont(nameFont_); display_.setTextSize(scale);
        display_.setTextDatum(middle_center); display_.setTextColor(White,0x2104);
        display_.drawString(m.toast,b.x+b.w/2,b.y+b.h/2);
    }
    display_.clearClipRect(); display_.setTextSize(1);
}
void Renderer::draw(const ScreenModel& m,const WatchData& watch) {
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    const auto start=esp_timer_get_time();
#endif
    if(!face_) return;
    if(m.screen!=previousScreen_ || m.toast!=previousToast_) full_=true;
    previousScreen_=m.screen; previousToast_=m.toast;
    ++layouts_;
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    frame_.begin(full_,capacity_);
#else
    frame_.begin(full_);
#endif
    face_->plan(frame_,display_,m,watch);
    planList(m);
    frame_.resolve();
    if(frame_.anyPaint()) {
        display_.startWrite(); display_.clearClipRect();
        if(frame_.full()) display_.fillScreen(0);
        else for(int i=0;i<frame_.count();++i) {
            const auto r=frame_.eraseBox(i);
            if(!r.empty()) display_.fillRect(r.x,r.y,r.w,r.h,0);
        }
        // Full fallback paints every view, even those whose add() returned -1.
        face_->paint(display_,frame_); paintList(m);
        display_.endWrite(); ++paints_;
    }
    if(frame_.overflow() && !overflowReported_) std::printf("[Renderer] element capacity exceeded: full repaint\n");
    overflowReported_=frame_.overflow();
    full_=frame_.overflow();
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    recordRender(m,start,esp_timer_get_time(),frame_.anyPaint(),layouts_);
#endif
}
}
