#pragma once
#include "apps/SettingsScreen.h"
namespace launcher {
class ScreenManager {
public:
    ScreenManager(int width=468,int height=468) {
        model_.width=width; model_.height=height; settings_.resize(width,height);
    }
    // Without a store and a clock the settings entry stays unavailable, exactly
    // as the unimplemented entries do. Nothing else in the launcher changes.
    void bind(SettingsStore* store,TimeService* time) { settings_.bind(store,time); }
    void setInfo(const char* name,const char* version,const char* idf) {
        settings_.setInfo(name,version,idf);
    }
    bool handle(const Events& e,TimeUs now);
    bool update(TimeUs now);
    // The settings values are read from the screen, never cached here: the
    // store is the only source of truth, so a save is visible the same frame.
    ScreenModel model() const {
        auto m=model_; m.animating=animating_;
        m.settings=settings_.model();
        m.brightness=settings_.brightness(); m.screenOffSec=settings_.screenOffSec();
        return m;
    }
    TimeUs nextUpdate() const;
    bool active() const { return model_.dragging || animating_; }
private:
    void animate(float transition,float scroll,TimeUs now);
    void settleList(float velocity,TimeUs now);
    SettingsScreen settings_;
    ScreenModel model_{};
    enum class Drag { None,Watch,List,Return } drag_=Drag::None;
    bool animating_=false;
    bool listSettling_=false,stopTouch_=false;
    float scrollTangent_=0;
    float fromTransition_=0,toTransition_=0,fromScroll_=0,toScroll_=0;
    float dragScroll_=0,dragTransition_=0;
    TimeUs animationStart_=0,nextFrame_=INT64_MAX,toastUntil_=INT64_MAX;
};
}
