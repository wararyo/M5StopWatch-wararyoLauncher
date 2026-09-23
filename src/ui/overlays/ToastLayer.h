#pragma once
#include "ui/Element.h"
#include "ui/Viewport.h"
#include "ui/graphics/Gfx.h"
namespace launcher {
// The short notice over the lower part of the screen. Above every screen and
// below the statistics chip. How long a notice stays is its sender's business
// (ScreenManager); this only places, shortens and paints it.
//
// A notice appearing, changing or going away turns the frame into a full
// repaint: the stopwatch panel it covers is background that only a full
// repaint restores (StopwatchLayer).
class ToastLayer {
public:
    void begin(const lgfx::IFont* font) { font_=font; }
    void plan(FramePlan& frame,Gfx& g,Viewport viewport,const char* text);
    void paint(Gfx& g,const FramePlan& frame,Viewport viewport);
private:
    const lgfx::IFont* font_=nullptr;
    Element element_;
    Rect box_{};
    int handle_=-1;
    // What the previous frame showed, by content: a pointer would call two
    // equal notices different, and one reused buffer the same.
    bool shown_=false;
    uint32_t shownHash_=0;
    char fitted_[96]{};
};
}
