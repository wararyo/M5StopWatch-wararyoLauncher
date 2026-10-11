#pragma once
#include "ui/rendering/Element.h"
#include "ui/rendering/Renderer.h"
#include "ui/graphics/Gfx.h"
#include "features/settings/SettingsLayout.h"
namespace launcher {
// The editor and information views as elements: title, fields, separators,
// information lines, actions and buttons, placed by SettingsLayout.h. Not a
// layer of its own: settings and the tutorial each own one and plan and paint
// it as part of theirs (docs/task15/plan.md 2.2).
class SettingsEditorView {
public:
    void begin(const lgfx::IFont* font) { font_=font; }
    // Another set of elements took over these pixels: nothing is compared
    // against a box from before.
    void invalidate() { for (auto& element:elements_) element=Element{}; }
    // `stats` is whether the statistics overlay is already on, which the
    // information view's action shows as taken.
    void plan(FramePlan& frame,Viewport viewport,const SettingsEditorModel& model,bool stats);
    void paint(Gfx& g,const PaintContext& context);
private:
    // The busiest view is the date editor: title, five fields, three
    // separators and two buttons. Information needs six: title, three lines,
    // one action and one button.
    static constexpr int Capacity=11;
    enum Kind { Title,Field,Separator,Button,InfoLine,Action };
    struct Item {
        Rect box{};
        Kind kind=Title;
        int index=0;
        bool selected=false;
        // An action already taken. Its text does not change, so this has to
        // reach the fingerprint or the colour change would not repaint.
        bool done=false;
        char text[40]{};
    };
    void build(const SettingsEditorModel& model,bool stats);
    Item items_[Capacity]{};
    Element elements_[Capacity]{};
    int count_=0;
    const lgfx::IFont* font_=nullptr;
    // What the last plan placed, which paint draws.
    SettingsGeometry geometry_{};
};
}
