#include "Renderer.h"
#include "VlwFont.h"
#include <cstdio>
#include <cstring>
#include <esp_timer.h>
#ifdef LAUNCHER_RENDER_METRICS
#include "RenderDiagnostics.h"
#endif
namespace launcher {
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
    appList_.begin(nameFont_); settings_.begin(nameFont_); toast_.begin(nameFont_);
    registerFace(digital_);
    return display_.width()>0 && display_.height()>0 && selectFace("digital",disableCache);
}
void Renderer::draw(const ScreenModel& m,const WatchData& watch) {
#ifdef LAUNCHER_RENDER_METRICS
    const auto start=esp_timer_get_time();
#endif
    if(!face_) return;
    // Only when the overlay is on: an unused build pays nothing for it, and the
    // frame it times is the one below the chip.
    const TimeUs statsStart=m.stats ? esp_timer_get_time() : 0;
    // A notice coming or going forces its own full repaint (ToastLayer).
    if(m.screen!=previousScreen_) full_=true;
    previousScreen_=m.screen;
    ++layouts_;
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    frame_.begin(full_,capacity_);
#else
    frame_.begin(full_);
#endif
    face_->plan(frame_,display_,m.homeRegion,watch);
    const bool listHidden=m.screen==ScreenId::Settings || m.screen==ScreenId::External ||
        m.screen==ScreenId::Stopwatch;
    appList_.plan(frame_,display_,m.viewport(),m,!listHidden);
    settings_.plan(frame_,display_,m.viewport(),m.settings,
                   m.screen==ScreenId::Settings,m.stats,nameFont_);
    external_.plan(frame_,display_,m.viewport(),m.external,
                   m.screen==ScreenId::External,nameFont_);
    stopwatch_.plan(frame_,display_,m.viewport(),m.stopwatch,
                    m.screen==ScreenId::Stopwatch,nameFont_);
    toast_.plan(frame_,display_,m.viewport(),m.toast);
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
        face_->paint(display_,frame_); appList_.paint(display_,frame_);
        settings_.paint(display_,frame_,m.viewport(),m.settings,
                    m.screen==ScreenId::Settings,nameFont_);
        external_.paint(display_,frame_,m.viewport(),
                    m.screen==ScreenId::External,nameFont_);
        stopwatch_.paint(display_,frame_,m.viewport(),
                     m.screen==ScreenId::Stopwatch,nameFont_); toast_.paint(display_,frame_,m.viewport());
        // Last of all, and outside the plan: the chip owns its own rectangle
        // and pushes it whole, so nothing below can leave it half erased. It is
        // given the frame's dirty box so it can leave its pixels alone when
        // nothing reached them (see StatsOverlay).
        if(m.stats && !statsSuppressed_)
            stats_.paint(display_,m.viewport(),frame_.full() ? Rect{0,0,m.width,m.height} : frame_.dirtyBounds());
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
#ifdef LAUNCHER_RENDER_METRICS
    recordRender(m,start,esp_timer_get_time(),frame_.anyPaint(),layouts_);
#endif
}
}
