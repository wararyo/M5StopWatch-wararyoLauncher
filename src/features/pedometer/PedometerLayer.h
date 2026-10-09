#pragma once
#include "ui/rendering/Element.h"
#include "ui/rendering/Renderer.h"
#include "ui/graphics/Gfx.h"
#include "ui/graphics/VlwGlyphs.h"
#include "ui/rendering/Viewport.h"
#include "features/pedometer/PedometerModel.h"
namespace launcher {
// The pedometer screen on a black ground (docs/Images/Pedometer): the icon,
// the title, the count with its unit, and OK. Each is an element, so a new
// count repaints only the count.
class PedometerLayer final : public RenderLayer {
public:
    // `names` for the Japanese title and unit, `text` for OK and `digits` for
    // the count; a missing one falls back to a built-in font.
    void begin(const lgfx::IFont* names,const lgfx::IFont* text,const VlwGlyphs* digits) {
        names_=names; text_=text; digits_=digits;
    }
    void prepare(Viewport viewport,const PedometerModel& model,bool visible) {
        viewport_=viewport; model_=model; visible_=visible;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
private:
    enum Part : uint8_t { Icon,Title,Count,Ok,PartCount };
    void paintCount(Gfx& g,const Rect& box) const;
    void setNames(Gfx& g) const;
    Element elements_[PartCount]{};
    bool shown_=false;
    const lgfx::IFont* names_=nullptr;
    const lgfx::IFont* text_=nullptr;
    const VlwGlyphs* digits_=nullptr;
    Viewport viewport_{};
    PedometerModel model_{};
    bool visible_=false;
};
}
