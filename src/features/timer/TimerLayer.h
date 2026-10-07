#pragma once
#include "ui/rendering/Element.h"
#include "ui/rendering/Renderer.h"
#include "ui/graphics/Gfx.h"
#include "ui/graphics/VlwGlyphs.h"
#include "features/timer/TimerLayout.h"
namespace launcher {
// The timer screen's three views on a black ground (docs/Images/Timer). Every
// box, keys and buttons included, is an element: the screen is mostly still,
// so a press repaints the field or the button it changed, a second only the
// time, and RESET's fill only RESET. Switching views is a full repaint.
class TimerLayer final : public RenderLayer {
public:
    // `names` for the Japanese label, `text` for the ASCII ones and `digits`
    // for the time; a missing one falls back to a built-in font.
    void begin(const lgfx::IFont* names,const lgfx::IFont* text,const VlwGlyphs* digits) {
        names_=names; text_=text; digits_=digits;
    }
    void prepare(Viewport viewport,const TimerModel& model,bool visible) {
        viewport_=viewport; model_=model; visible_=visible;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
private:
    // The setup holds the most: icon, three fields, two colons, ten keys, SET.
    static constexpr int Capacity=17;
    enum Kind : uint8_t { Icon,Field,Colon,Key,Button,Time,Dismiss };
    struct Item {
        Rect box{};
        Kind kind=Icon;
        uint16_t fill=0,ink=0,glow=0;
        uint16_t progress=0; // RESET's fill, 0..1000.
        char text[16]{};
    };
    void build();
    void drawDigits(Gfx& g,const char* text,const Rect& box,uint16_t ink,uint16_t ground) const;
    void drawLabel(Gfx& g,const lgfx::IFont* font,const char* text,const Rect& box,uint16_t ink,uint16_t ground,
                   bool blend=false) const;
    Item items_[Capacity]{};
    Element elements_[Capacity]{};
    int count_=0;
    // The view the elements were last planned for; another view is a full repaint.
    int shown_=-1;
    const lgfx::IFont* names_=nullptr;
    const lgfx::IFont* text_=nullptr;
    const VlwGlyphs* digits_=nullptr;
    Viewport viewport_{};
    TimerModel model_{};
    bool visible_=false;
};
}
