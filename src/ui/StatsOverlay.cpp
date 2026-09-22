#include "StatsOverlay.h"
#include "ListLayout.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace launcher {
namespace {
constexpr uint16_t Chip=0x2104,Ink=0xb7e0; // The toast panel and the list's lime.
constexpr TimeUs WindowUs=500000;
// Size 1 of the built-in 6x8 font would be 0.55mm tall on a 366ppi panel, so 2
// is the smallest that can actually be read. It is the one number to turn down
// if the chip ever costs measurable time.
constexpr float TextSize=2;
constexpr int GlyphW=6,GlyphH=8,Columns=7,Lines=2,PadX=4,PadY=3;
constexpr int ChipW=int(Columns*GlyphW*TextSize)+2*PadX;
constexpr int ChipH=int(Lines*GlyphH*TextSize)+2*PadY;
constexpr int LineH=int(GlyphH*TextSize);
// Square corners, not rounded: the sprite is pushed as a whole rectangle, so
// rounded corners would have to carry a guessed background and would show as
// notches over the stopwatch panel.
//
// Anchored by its right edge in the 468 basis: clear of the stopwatch buttons
// above, which end at y=127, and inside the bezel at every corner (the farthest
// is 207 of the 234 radius).
Rect chipBox(const ScreenModel& m) {
    return {offsetPx(m,418)-ChipW,offsetPx(m,140),ChipW,ChipH};
}
float capped(float value) { return std::clamp(value,0.0f,99.9f); }
}
void StatsOverlay::record(TimeUs start,TimeUs end) {
    if (!windowStart_) { windowStart_=end; frames_=0; drawTotal_=0; return; }
    ++frames_; drawTotal_+=end-start;
    const TimeUs elapsed=end-windowStart_;
    if (elapsed<WindowUs) return;
    char fps[12],draw[12];
    // Completion interval, as docs/plan.md 6.3 defines the real fps: a lone
    // frame after a long gap widens the window and reads low, as it should.
    std::snprintf(fps,sizeof(fps),"%4.1ffps",capped(float(frames_)*1000000.0f/float(elapsed)));
    std::snprintf(draw,sizeof(draw),"%4.1fms",capped(float(drawTotal_)/float(frames_)/1000.0f));
    if (std::strcmp(fps,fps_)!=0 || std::strcmp(draw,draw_)!=0) {
        std::memcpy(fps_,fps,sizeof(fps_)); std::memcpy(draw_,draw,sizeof(draw_)); stale_=true;
    }
    windowStart_=end; frames_=0; drawTotal_=0;
}
void StatsOverlay::render() {
    cache_.fillScreen(Chip);
    cache_.setFont(&fonts::Font0); cache_.setTextSize(TextSize);
    cache_.setTextDatum(top_right); cache_.setTextColor(Ink,Chip);
    cache_.drawString(fps_,ChipW-PadX,PadY);
    cache_.drawString(draw_,ChipW-PadX,PadY+LineH);
}
void StatsOverlay::paint(M5GFX& g,const ScreenModel& m,const Rect& dirty) {
    const Rect b=chipBox(m);
    // Nothing touched the chip and it still reads right: leave the pixels
    // alone. Writing them would widen the frame's flush rectangle for no gain.
    if (shown_ && !stale_ && !dirty.intersects(b)) return;
    if (!cacheTried_) {
        cacheTried_=true;
        cache_.setPsram(false); cache_.setColorDepth(16);
        cacheReady_=cache_.createSprite(ChipW,ChipH)!=nullptr;
        std::printf("[Stats] overlay cache=%s bytes=%d internal\n",
                    cacheReady_ ? "ready" : "direct",ChipW*ChipH*2);
    }
    if (cacheReady_) {
        if (stale_) { render(); stale_=false; }
        cache_.pushSprite(&g,b.x,b.y);
        shown_=true;
        return;
    }
    // No cache: draw straight to the panel rather than lose the reading.
    g.setClipRect(b.x,b.y,b.w,b.h);
    g.fillRect(b.x,b.y,b.w,b.h,Chip);
    g.setFont(&fonts::Font0); g.setTextSize(TextSize);
    g.setTextDatum(top_right); g.setTextColor(Ink,Chip);
    g.drawString(fps_,b.x+b.w-PadX,b.y+PadY);
    g.drawString(draw_,b.x+b.w-PadX,b.y+PadY+LineH);
    g.clearClipRect(); g.setTextSize(1);
    stale_=false; shown_=true;
}
}
