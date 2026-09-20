#pragma once
#include "ui/DisplayModel.h"
namespace launcher {
class ScreenManager {
public:
    ScreenManager(int width=468,int height=468) { model_.width=width; model_.height=height; }
    bool handle(const Events& e,TimeUs now);
    bool update(TimeUs now);
    ScreenModel model() const { auto m=model_; m.animating=animating_; return m; }
    TimeUs nextUpdate() const;
    bool active() const { return model_.dragging || animating_; }
private:
    void animate(float transition,float scroll,TimeUs now);
    ScreenModel model_{};
    enum class Drag { None,Watch,List,Return } drag_=Drag::None;
    bool animating_=false;
    float fromTransition_=0,toTransition_=0,fromScroll_=0,toScroll_=0;
    float dragScroll_=0,dragTransition_=0;
    TimeUs animationStart_=0,nextFrame_=INT64_MAX,toastUntil_=INT64_MAX;
};
}
