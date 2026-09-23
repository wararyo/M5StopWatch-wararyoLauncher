#pragma once
#include "ExternalLayout.h"
#include "WatchFace.h"
namespace launcher {
// The external app detail layer. Like the settings layer it registers nothing
// at all while its screen is closed, so the frame budget is only spent when the
// screen is actually open.
class ExternalLayer {
public:
    void plan(FramePlan& frame,Gfx& g,Viewport viewport,const ExternalModel& model,
              bool visible,const lgfx::IFont* font);
    void paint(Gfx& g,const FramePlan& frame,Viewport viewport,bool visible,const lgfx::IFont* font);
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
    int handles_[Capacity]{};
    int count_=0;
};
}
