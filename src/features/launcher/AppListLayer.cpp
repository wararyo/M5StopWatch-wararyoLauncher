#include "AppListLayer.h"
#include "assets/AppIcons.h"
#include "AppListLayout.h"
namespace launcher {
void AppListLayer::plan(FramePlan& frame,Gfx& g) {
    const ListRows rows=buildAppListRows(model_,rows_,appIcon);
    view_.plan(frame,g,appListPlacement(viewport_,model_.transition,model_.list.scroll),rows,model_.list,
               visible_,background_);
}
void AppListLayer::paint(Gfx& g,const PaintContext& context) {
    view_.paint(g,context.within(appListCover(viewport_,model_.transition)));
}
}
