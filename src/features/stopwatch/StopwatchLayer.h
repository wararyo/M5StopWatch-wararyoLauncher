#pragma once
#include "ui/rendering/Element.h"
#include "ui/graphics/Gfx.h"
#include "StopwatchLayout.h"
#include "ui/rendering/Renderer.h"
namespace launcher {
// The stopwatch layer. The rounded panel and its divider are NOT plan elements:
// they are background, painted only on a full repaint. Registering the panel
// would make every hundredth-of-a-second repaint spread through
// FramePlan::resolve to the panel and from there to the whole screen, so the
// 40 Hz update would cost a full frame.
//
// What makes that safe: every element below has a FIXED box and paints the
// panel colour across the whole of it before drawing, so the black erase that
// Renderer does on the old box is covered within the same frame. The only other
// thing that can touch these pixels is the toast, and a toast appearing or
// disappearing already forces a full repaint (ToastLayer).
class StopwatchLayer final : public RenderLayer {
public:
    void begin(const lgfx::IFont* font) { font_=font; }
    void prepare(Viewport viewport,const StopwatchModel& model,bool visible) {
        viewport_=viewport; model_=model; visible_=visible;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const FramePlan& frame) override;
private:
    // Clock, hundredths, two buttons, three lap rows.
    static constexpr int Capacity=4+StopwatchLapRows;
    enum Kind { Clock,Fraction,Button,Lap,NoLap };
    struct Item {
        Rect box{};
        Kind kind=Clock;
        int index=0;
        uint16_t fill=0,ink=0;
        char text[24]{},trailing[24]{};
    };
    void build(Viewport viewport,const StopwatchModel& model);
    Item items_[Capacity]{};
    Element elements_[Capacity]{};
    int handles_[Capacity]{};
    int count_=0;
    float timeSize_=1;
    const lgfx::IFont* font_=nullptr;
    Viewport viewport_{};
    StopwatchModel model_{};
    bool visible_=false;
};
}
