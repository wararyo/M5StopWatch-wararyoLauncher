#pragma once
#include "DisplayModel.h"
#include "Geometry.h"
#include <M5Unified.h>
namespace launcher {
// The on-screen frame statistics of docs/plan.md 6.3, for reading real fps on a
// normal build without a serial console.
//
// Deliberately OUTSIDE the FramePlan. A value that changes every window would
// drag every element it overlaps into a repaint through FramePlan::resolve, so
// the act of measuring would move the number being measured. Instead the chip
// is opaque and pushes itself whole at the end of every painted frame, after
// every layer, so nothing below it can leave it half erased.
//
// The glyphs are rendered into an internal-RAM sprite only when the text
// changes, once per window. Drawing the built-in font at double size costs a
// few hundred small fills, which measured as +2.2ms per frame on the app list
// and is exactly what this cache removes.
//
// The push itself is then skipped unless the frame actually disturbed the
// chip's pixels. This panel flushes ONE bounding box per frame, so pushing
// where nothing else was drawn enlarges that box and costs transfer time; that
// was the remaining +1.2ms. When something did paint across the chip, the box
// already covers it and the push is free.
//
// It registers no update deadline either: a still screen draws nothing, the
// window does not advance and the last value simply stays. Still means 0 fps,
// so there is nothing to show anyway.
class StatsOverlay {
public:
    // Only for frames that actually painted. `end` is taken after endWrite, so
    // the millisecond figure covers the panel flush like [RenderDiag] does.
    void record(TimeUs start,TimeUs end);
    // Inside the frame's startWrite/endWrite, after every layer. `dirty` is
    // everything the frame erased and repainted.
    void paint(M5GFX& g,const ScreenModel& m,const Rect& dirty);
private:
    void render();
    // Both lines hold seven characters, so the chip never resizes as the
    // numbers change and its rectangle is a constant.
    char fps_[12]="  --fps",draw_[12]="  --ms";
    M5Canvas cache_;
    bool cacheTried_=false,cacheReady_=false,stale_=true,shown_=false;
    TimeUs windowStart_=0,drawTotal_=0;
    uint32_t frames_=0;
};
}
