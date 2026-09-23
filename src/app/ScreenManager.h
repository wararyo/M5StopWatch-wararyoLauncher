#pragma once
#include "app/AppRegistry.h"
#include "apps/ExternalAppScreen.h"
#include "apps/SettingsScreen.h"
#include "apps/StopwatchScreen.h"
#include "app/FrameModel.h"
#include "app/EffectiveSettings.h"
#include "features/launcher/AppListRows.h"
#include "ui/list/ListController.h"
#include <array>
namespace launcher {
// It implements BootShutdown itself: it owns the stopwatch service, and the
// launch that has to end a measurement is the one it started (plan.md 8.2).
class ScreenManager final : public BootShutdown {
public:
    ScreenManager(int width=468,int height=468) {
        nav_.width=width; nav_.height=height;
        settings_.resize(width,height); external_.resize(width,height);
        stopwatchScreen_.resize(width,height); stopwatchScreen_.bind(&stopwatch_);
        list_.resize({width,height});
        // Input reads only the ids, which never change; names are drawn from
        // the frame's model instead.
        list_.setRows(buildAppListRows(AppListModel{},rows_));
    }
    // The list borrows rows_, so a copy would point into the original.
    ScreenManager(const ScreenManager&)=delete;
    ScreenManager& operator=(const ScreenManager&)=delete;
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
    // Deadline and activity belong to whatever is on screen: an open screen
    // answers for itself, and the launcher only while it is showing, so a
    // hidden list's motion never keeps settings awake or drawing.
    TimeUs nextUpdate() const;
    bool active() const { return active_ ? active_->active() : launcherActive(); }
private:
    // Home to list and back. The list's own scrolling is ListController's.
    void animateTransition(float transition,TimeUs now);
    void stopTransition() { transitionAnimating_=false; transitionFrame_=INT64_MAX; }
    bool launcherActive() const { return drag_!=Drag::None || transitionAnimating_ || list_.active(); }
    FrameActivity activity() const;
    bool open(AppScreen& screen,ScreenId id,TimeUs now);
    SettingsScreen settings_;
    RuntimeSettings runtimeSettings_{};
    ExternalAppScreen external_;
    StopwatchService stopwatch_;
    StopwatchScreen stopwatchScreen_;
    SlotCatalog slots_{};
    AppScreen* active_=nullptr;
    struct NavigationState {
        ScreenId screen=ScreenId::Home;
        int width=468,height=468;
        uint32_t homeCount=0;
        const char* toast=nullptr;
        float transition=0;
        Viewport viewport() const { return {width,height}; }
    } nav_{};
    ListController list_;
    std::array<ListRow,AppListCount> rows_{};
    // Who owns the current drag, fixed when it starts: the clock's pull up,
    // the list's pull back home from its top, or the list's own scrolling.
    enum class Drag { None,Watch,List,Return } drag_=Drag::None;
    bool transitionAnimating_=false;
    float fromTransition_=0,toTransition_=0,dragTransition_=0;
    TimeUs transitionStart_=0,transitionFrame_=INT64_MAX,toastUntil_=INT64_MAX;
    // Last time the manager was driven, so the boot shutdown callback can end
    // the measurement without the API handing it a clock.
    TimeUs now_=0;
};
}
