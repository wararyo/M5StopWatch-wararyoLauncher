#pragma once
#include "features/launcher/AppListModel.h"
#include "features/launcher/AppListRows.h"
#include "ui/list/ListView.h"
#include "ui/rendering/Renderer.h"
namespace launcher {
// The app list as drawn: registry and slot names turned into rows, the slide
// over the clock turned into a placement, and the shared ListView doing the
// rest. The launcher's own knowledge stops here.
class AppListLayer final : public RenderLayer {
public:
    void begin(const lgfx::IFont* font) { view_.begin(font); }
    // Hidden, the list still registers its slots empty, so whatever it painted
    // last is erased.
    void prepare(Viewport viewport,const AppListModel& model,bool visible) {
        viewport_=viewport; model_=model; visible_=visible;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const FramePlan& frame) override { view_.paint(g,frame); }
    ListView& view() { return view_; }
    const ListView& view() const { return view_; }
private:
    ListView view_;
    Viewport viewport_{};
    AppListModel model_{};
    bool visible_=false;
    // A member, not a local: the view reads the rows again at paint.
    std::array<ListRow,AppListCount> rows_{};
};
}
