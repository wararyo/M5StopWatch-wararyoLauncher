#pragma once
#include "app/AppRegistry.h"
#include "apps/ExternalAppScreen.h"
#include "apps/SettingsScreen.h"
namespace launcher {
class ScreenManager {
public:
    ScreenManager(int width=468,int height=468) {
        model_.width=width; model_.height=height;
        settings_.resize(width,height); external_.resize(width,height);
    }
    // Without a store and a clock the settings entry stays unavailable, exactly
    // as the unimplemented entries do. Nothing else in the launcher changes.
    void bind(SettingsStore* store,TimeService* time) { settings_.bind(store,time); }
    void bindSlots(SlotService* slots) { external_.bind(slots,&slots_); }
    void setInfo(const char* name,const char* version,const char* idf) {
        settings_.setInfo(name,version,idf);
    }
    // A scan result only ever updates the catalog. It never moves the screen or
    // starts a boot, so a result arriving after home is harmless (plan.md 8.2).
    void setSlots(const SlotCatalog& slots) { slots_=slots; }
    bool handle(const Events& e,TimeUs now);
    bool update(TimeUs now);
    // Issued by the runtime once the committing frame has been painted.
    bool commitPendingBoot() { return external_.commitPendingBoot(); }
    // The settings values are read from the screen, never cached here: the
    // store is the only source of truth, so a save is visible the same frame.
    ScreenModel model() const;
    TimeUs nextUpdate() const;
    bool active() const { return model_.dragging || animating_; }
private:
    void animate(float transition,float scroll,TimeUs now);
    void settleList(float velocity,TimeUs now);
    bool open(AppScreen& screen,ScreenId id);
    SettingsScreen settings_;
    ExternalAppScreen external_;
    SlotCatalog slots_{};
    AppScreen* active_=nullptr;
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
