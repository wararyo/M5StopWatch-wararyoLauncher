#include "Renderer.h"
#include "IconSet.h"
#include "Text.h"
#include "VlwFont.h"
#include "app/AppRegistry.h"
#include <cstdio>
#include <cstring>
#include <esp_timer.h>
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "RenderDiagnostics.h"
#endif
namespace launcher {
namespace {
constexpr uint16_t White=0xf7be,Lime=0xb7e0,Dimmed=0x7bcf;
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
    // An app screen covers the list rather than sliding it away, so the rows
    // are planned empty and their last painted boxes still get erased.
    const bool hidden=m.screen==ScreenId::Settings || m.screen==ScreenId::External ||
        m.screen==ScreenId::Stopwatch;
    for(int i=0;i<5;++i) {
        auto& row=plannedRows_[i]; row.layout=hidden ? RowLayout{} : layoutRow(m,i);
        const auto& box=row.layout.box;
        if(box.empty()) {
            row.name[0]=0;
            row.handle=frame_.add(rows_[i],{},0);
            continue;
        }
        display_.setFont(nameFont_); display_.setTextSize(scale);
        // A width that does not depend on the row's position, so a name is
        // shortened only when it cannot fit the screen at all.
        fitText(display_,m.names[i] ? m.names[i] : AppRegistry[i].name,row.name,sizeof(row.name),labelWidth(m));
        uint32_t hash=hashValue(uint32_t(i==m.selection)|(uint32_t(m.rowDimmed[i])<<1),
                                hashString(row.name));
        hash=hashValue(row.layout.centerY,hashValue(row.layout.iconX,hash));
        row.handle=frame_.add(rows_[i],box,hash);
    }
}
void Renderer::planToast(const ScreenModel& m) {
    if(!m.toast) { toastBox_={}; toastHandle_=frame_.add(toast_,{},0); return; }
    display_.setFont(nameFont_); display_.setTextSize(float(std::min(m.width,m.height))/468);
    const int height=scaled(m,46),centreY=scaled(m,360);
    // Width follows the text: the settings notices are three times as long as
    // "準備中" and were being cut off by a box sized for the short one.
    // Clamped to the chord at the lower edge so the bezel never crops it.
    const int radius=std::min(m.width,m.height)/2;
    const int dy=std::abs(centreY+height/2-m.height/2);
    const int chord=int(std::sqrt(float(radius)*radius-float(dy)*dy))-scaled(m,6);
    const int width=std::min(2*chord,int(display_.textWidth(m.toast))+2*scaled(m,22));
    toastBox_={m.width/2-width/2,centreY-height/2,width,height};
    toastHandle_=frame_.add(toast_,toastBox_,hashString(m.toast));
    display_.setTextSize(1);
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
        display_.setTextDatum(middle_left);
        // A slot that cannot be launched greys its name out. The icon is left
        // alone so the rows still scan as one column.
        display_.setTextColor(i==m.selection ? Lime : (m.rowDimmed[i] ? Dimmed : White),0);
        display_.drawString(row.name,r.labelX,r.centerY);
    }
    display_.clearClipRect(); display_.setTextSize(1);
}
void Renderer::paintToast(const ScreenModel& m) {
    if(toastBox_.empty() || !frame_.shouldPaint(toastHandle_)) return;
    const auto& b=toastBox_;
    display_.setClipRect(b.x,b.y,b.w,b.h);
    display_.fillRoundRect(b.x,b.y,b.w,b.h,scaled(m,14),0x2104);
    display_.setFont(nameFont_); display_.setTextSize(float(std::min(m.width,m.height))/468);
    // A notice too long even for the chord is shortened, never silently clipped.
    char fitted[96];
    fitText(display_,m.toast,fitted,sizeof(fitted),b.w-2*scaled(m,12));
    display_.setTextDatum(middle_center); display_.setTextColor(White,0x2104);
    display_.drawString(fitted,b.x+b.w/2,b.y+b.h/2);
    display_.clearClipRect(); display_.setTextSize(1);
}
void Renderer::draw(const ScreenModel& m,const WatchData& watch) {
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    const auto start=esp_timer_get_time();
#endif
    if(!face_) return;
    // Only when the overlay is on: an unused build pays nothing for it, and the
    // frame it times is the one below the chip.
    const TimeUs statsStart=m.stats ? esp_timer_get_time() : 0;
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
    settings_.plan(frame_,display_,m,nameFont_);
    external_.plan(frame_,display_,m,nameFont_);
    stopwatch_.plan(frame_,display_,m,nameFont_);
    planToast(m);
    frame_.resolve();
    if(frame_.anyPaint()) {
        display_.startWrite(); display_.clearClipRect();
        if(frame_.full()) display_.fillScreen(0);
        else for(int i=0;i<frame_.count();++i) {
            const auto r=frame_.eraseBox(i);
            if(!r.empty()) display_.fillRect(r.x,r.y,r.w,r.h,0);
        }
        // Full fallback paints every view, even those whose add() returned -1.
        // The toast is the topmost layer: it was being drawn before settings,
        // so the save and cancel buttons landed on top of the notice.
        face_->paint(display_,frame_); paintList(m);
        settings_.paint(display_,frame_,m,nameFont_);
        external_.paint(display_,frame_,m,nameFont_);
        stopwatch_.paint(display_,frame_,m,nameFont_); paintToast(m);
        // Last of all, and outside the plan: the chip owns its own rectangle
        // and pushes it whole, so nothing below can leave it half erased. It is
        // given the frame's dirty box so it can leave its pixels alone when
        // nothing reached them (see StatsOverlay).
        if(m.stats && !statsSuppressed_)
            stats_.paint(display_,m,frame_.full() ? Rect{0,0,m.width,m.height} : frame_.dirtyBounds());
        display_.endWrite(); ++paints_;
        // After endWrite, which is where this panel flushes the modified region
        // over QSPI. [RenderDiag] measures the same span, so the two agree.
        // The chip therefore shows the previous window's text, which is what it
        // would show anyway between updates.
        if(m.stats && !statsSuppressed_) stats_.record(statsStart,esp_timer_get_time());
    }
    if(frame_.overflow() && !overflowReported_) std::printf("[Renderer] element capacity exceeded: full repaint\n");
    overflowReported_=frame_.overflow();
    full_=frame_.overflow();
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    recordRender(m,start,esp_timer_get_time(),frame_.anyPaint(),layouts_);
#endif
}
}
