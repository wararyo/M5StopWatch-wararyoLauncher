#pragma once
#include "ExternalLayout.h"
#include "ui/rendering/Element.h"
#include "ui/graphics/Gfx.h"
#include "ui/rendering/Renderer.h"
namespace launcher {
// The external app detail layer. Like the settings layer it registers nothing
// at all while its screen is closed, so the frame budget is only spent when the
// screen is actually open.
class ExternalLayer final : public RenderLayer {
public:
    void begin(const lgfx::IFont* font) { font_=font; }
    void prepare(Viewport viewport,const ExternalModel& model,bool visible) {
        viewport_=viewport; model_=model; visible_=visible;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
private:
    // Title, three detail lines and two buttons.
    static constexpr int Capacity=6;
    enum Kind { Title,Line,Button };
    struct Item {
        Rect box{};
        Kind kind=Title;
        int index=0;
        bool selected=false,alert=false;
        char text[64]{};
    };
    void build(Gfx& g,const lgfx::IFont* font,Viewport viewport,const ExternalModel& model);
    Item items_[Capacity]{};
    Element elements_[Capacity]{};
    int count_=0;
    const lgfx::IFont* font_=nullptr;
    Viewport viewport_{};
    ExternalModel model_{};
    bool visible_=false;
};
}
