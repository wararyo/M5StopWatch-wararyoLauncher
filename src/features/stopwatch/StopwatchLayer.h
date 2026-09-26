#pragma once
#include "ui/rendering/Element.h"
#include "ui/graphics/Gfx.h"
#include "StopwatchLayout.h"
#include "ui/rendering/Renderer.h"
namespace launcher {
// The stopwatch layer. The rounded panel and its divider are NOT plan elements:
// they are background, repainted inside whatever the frame restores. They
// never change on their own, so they declare no damage, and a hundredth of a
// second repaints only its own box (docs/task10/plan-10-4.md 1).
class StopwatchLayer final : public RenderLayer {
public:
    void begin(const lgfx::IFont* font) { font_=font; }
    void prepare(Viewport viewport,const StopwatchModel& model,bool visible) {
        viewport_=viewport; model_=model; visible_=visible;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
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
    int count_=0;
    float timeSize_=1;
    const lgfx::IFont* font_=nullptr;
    Viewport viewport_{};
    StopwatchModel model_{};
    bool visible_=false;
};
}
