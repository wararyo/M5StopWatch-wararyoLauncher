#include "AppListLayer.h"
#include "AppIcons.h"
#include "AppListLayout.h"
namespace launcher {
void AppListLayer::plan(FramePlan& frame,Gfx& g,Viewport viewport,const AppListModel& model,bool visible) {
    const ListRows rows=buildAppListRows(model,rows_,appIcon);
    view_.plan(frame,g,appListPlacement(viewport,model.transition,model.list.scroll),rows,model.list,visible);
}
}
