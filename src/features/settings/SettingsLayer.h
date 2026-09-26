#pragma once
#include "ui/rendering/Element.h"
#include "ui/graphics/Gfx.h"
#include "ui/list/ListView.h"
#include "features/settings/SettingsMenu.h"
#include "SettingsLayout.h"
#include "ui/rendering/Renderer.h"
#include <array>
namespace launcher {
// The settings layer. It plans nothing at all on the clock and the app list, so
// the frame cost is only paid while settings is open.
//
// The top menu is the shared list, drawn by a ListView of its own; editors and
// information are the elements below. Only one of the two is planned in a
// frame. Switching between them keeps the screen id, so the app's screen
// change never sees it: the layer itself turns that frame into a full repaint
// and forgets both histories.
class SettingsLayer final : public RenderLayer {
public:
    void begin(const lgfx::IFont* font) { font_=font; menu_.begin(font); }
    // `stats` is whether the statistics overlay is already on, which the
    // information view's action shows as taken.
    void prepare(Viewport viewport,const SettingsModel& model,bool visible,bool stats) {
        viewport_=viewport; model_=model; visible_=visible; stats_=stats;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
    const ListView& menuView() const { return menu_; }
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    ListView& menuViewForTest() { return menu_; }
    // Replaces the first row's label, so the render check reaches a name that
    // has to be shortened; the real labels are all short. nullptr restores it.
    void menuLabelForTest(const char* label) { labelForTest_=label; }
#endif
private:
    // The busiest view is the date editor: title, five fields, three
    // separators and two buttons. Information needs six: title, three lines,
    // one action and one button. The menu registers the list's slots instead.
    static constexpr int Capacity=11;
    enum Kind { Title,Field,Separator,Button,InfoLine,Action };
    struct Item {
        Rect box{};
        Kind kind=Title;
        int index=0;
        bool selected=false,editing=false;
        // An action already taken. Its text does not change, so this has to
        // reach the fingerprint or the colour change would not repaint.
        bool done=false;
        char text[40]{};
    };
    enum class Shown : uint8_t { None,Menu,Items };
    void build(Viewport viewport,const SettingsModel& model,bool stats);
    Item items_[Capacity]{};
    Element elements_[Capacity]{};
    int count_=0;
    Shown shown_=Shown::None;
    const lgfx::IFont* font_=nullptr;
    Viewport viewport_{};
    SettingsModel model_{};
    bool visible_=false,stats_=false;
    ListView menu_;
    // Members, not locals: the view reads the rows and labels again at paint.
    std::array<ListRow,SettingsMenuCount> rows_{};
    SettingsMenuLabels labels_{};
    const char* labelForTest_=nullptr;
};
}
