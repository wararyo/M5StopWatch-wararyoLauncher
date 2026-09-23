#pragma once
#include "features/launcher/AppListModel.h"
#include "features/launcher/AppListRows.h"
#include "ui/list/ListView.h"
namespace launcher {
// The app list as drawn: registry and slot names turned into rows, the slide
// over the clock turned into a placement, and the shared ListView doing the
// rest. The launcher's own knowledge stops here.
class AppListLayer {
public:
    void begin(const lgfx::IFont* font) { view_.begin(font); }
    void plan(FramePlan& frame,Gfx& g,Viewport viewport,const AppListModel& model,bool visible);
    void paint(Gfx& g,const FramePlan& frame) { view_.paint(g,frame); }
    ListView& view() { return view_; }
    const ListView& view() const { return view_; }
private:
    ListView view_;
    // A member, not a local: the view reads the rows again at paint.
    std::array<ListRow,AppListCount> rows_{};
};
}
