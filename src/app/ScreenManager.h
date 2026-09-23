#pragma once
#include "app/AppRegistry.h"
#include "apps/ExternalAppScreen.h"
#include "apps/SettingsScreen.h"
#include "apps/StopwatchScreen.h"
#include "app/FrameModel.h"
#include "app/EffectiveSettings.h"
namespace launcher {
// It implements BootShutdown itself: it owns the stopwatch service, and the
// launch that has to end a measurement is the one it started (plan.md 8.2).
class ScreenManager final : public BootShutdown {
public:
    ScreenManager(int width=468,int height=468) {
        model_.width=width; model_.height=height;
        settings_.resize(width,height); external_.resize(width,height);
        stopwatchScreen_.resize(width,height); stopwatchScreen_.bind(&stopwatch_);
    }
    // Without a store and a clock the settings entry stays unavailable, exactly
    // as the unimplemented entries do. Nothing else in the launcher changes.
    void bind(SettingsStore* store,TimeService* time) { settings_.bind(store,time,&runtimeSettings_); }
    void bindSlots(SlotService* slots) {
        external_.bind(slots,&slots_);
        if (slots) slots->bindShutdown(this);
    }
    // The only place a measurement ends without the user asking: the boot is
    // already certain here, and the value is not restored across the restart.
    void onBootCommitted() override { stopwatch_.stop(now_); }
    // Read-only: the manager owns the measurement, and a boot commit can end
    // it while the screen's own sample is necessarily stale.
    const StopwatchService& stopwatch() const { return stopwatch_; }
    void setInfo(const char* name,const char* version,const char* idf) {
        settings_.setInfo(name,version,idf);
    }
    // A scan result only ever updates the catalog. It never moves the screen or
    // starts a boot, so a result arriving after home is harmless (plan.md 8.2).
    void setSlots(const SlotCatalog& slots) { slots_=slots; }
    bool handle(const Events& e,TimeUs now);
    bool update(TimeUs now);
    // Issued by the runtime once the committing frame has been painted.
    bool commitPendingBoot(TimeUs now) { now_=now; return external_.commitPendingBoot(); }
    // The settings values are read from the screen, never cached here: the
    // store is the only source of truth, so a save is visible the same frame.
    ScreenModel model() const;
    EffectiveSettings effectiveSettings() const {
        return {settings_.brightness(),settings_.screenOffSec()};
    }
    TimeUs nextUpdate() const;
    bool active() const { return model_.dragging || animating_; }
private:
    void animate(float transition,float scroll,TimeUs now);
    void settleList(float velocity,TimeUs now);
    bool open(AppScreen& screen,ScreenId id,TimeUs now);
    SettingsScreen settings_;
    RuntimeSettings runtimeSettings_{};
    ExternalAppScreen external_;
    StopwatchService stopwatch_;
    StopwatchScreen stopwatchScreen_;
    SlotCatalog slots_{};
    AppScreen* active_=nullptr;
    struct NavigationState : AppListModel {
        ScreenId screen=ScreenId::Home;
        int width=468,height=468;
        uint32_t homeCount=0;
        const char* toast=nullptr;
        Viewport viewport() const { return {width,height}; }
    } model_{};
    enum class Drag { None,Watch,List,Return } drag_=Drag::None;
    bool animating_=false;
    bool listSettling_=false,stopTouch_=false;
    float scrollTangent_=0;
    float fromTransition_=0,toTransition_=0,fromScroll_=0,toScroll_=0;
    float dragScroll_=0,dragTransition_=0;
    TimeUs animationStart_=0,nextFrame_=INT64_MAX,toastUntil_=INT64_MAX;
    // Last time the manager was driven, so the boot shutdown callback can end
    // the measurement without the API handing it a clock.
    TimeUs now_=0;
};
}
