#pragma once
#include "host/LaunchRegistry.h"
#include "features/external/ExternalAppScreen.h"
#include "features/settings/SettingsScreen.h"
#include "features/stopwatch/StopwatchScreen.h"
#include "host/FrameModel.h"
#include "host/EffectiveSettings.h"
#include "features/launcher/AppListRows.h"
#include "features/launcher/LauncherController.h"
#include "features/home/HomeInteraction.h"
namespace launcher {
// Which screen is shown, entering and leaving screens, home ahead of anything
// else, opening what the launcher decided, and how long a notice stays. The
// measurement and the runtime settings belong to the application
// (host/HostApplication.h) and are only lent here: nothing a screen does to its own
// lifetime can end them.
class ScreenManager {
public:
    ScreenManager(StopwatchService& stopwatch,RuntimeSettings& runtime,int width=468,int height=468)
        :runtime_(runtime),viewport_{width,height},launcher_(viewport_) {
        settings_.resize(width,height); external_.resize(width,height);
        stopwatchScreen_.resize(width,height); stopwatchScreen_.bind(&stopwatch);
    }
    ScreenManager(const ScreenManager&)=delete;
    ScreenManager& operator=(const ScreenManager&)=delete;
    // Without a store and a clock the settings entry stays unavailable, exactly
    // as the unimplemented entries do. Nothing else in the launcher changes.
    void bind(SettingsStore* store,TimeService* time) { settings_.bind(store,time); }
    void bindSlots(SlotService* slots) { external_.bind(slots,&slots_); }
    // The clock layer's input. Without it a tap on the clock does nothing and
    // the list is still reached by A/B and the swipe up.
    void bindHome(HomeControlPort* home) { home_=home; }
    // Whether a touch starting now may become a long press on the clock.
    bool homeAtRest() const { return !active_ && launcher_.atRest(); }
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
    // The frame as the app composes it from each feature's model. The settings
    // values are read from the screen, never cached here: the store is the
    // only source of truth, so a save is visible the same frame.
    FrameModel model() const;
    EffectiveSettings effectiveSettings() const {
        return {settings_.brightness(),settings_.screenOffSec()};
    }
    Viewport viewport() const { return viewport_; }
    // Deadline and activity belong to whatever is on screen: an open screen
    // answers for itself, and the launcher only while it is showing, so a
    // hidden list's motion never keeps settings awake or drawing.
    TimeUs nextUpdate() const;
    bool active() const { return active_ ? active_->active() : launcher_.active(); }
private:
    FrameActivity activity() const;
    ScreenId screen() const { return active_ ? activeId_ : launcher_.listShown() ? ScreenId::AppList : ScreenId::Home; }
    // The launcher's decision, carried out: the entry's screen is entered, or
    // the input is acknowledged with a notice when there is none to enter.
    bool launch(const LaunchEntry* entry,TimeUs now);
    bool open(Screen& screen,ScreenId id,TimeUs now);
    void notify(const char* notice,TimeUs now) { toast_=notice; toastUntil_=now+1400000; }
    RuntimeSettings& runtime_;
    SettingsScreen settings_;
    ExternalAppScreen external_;
    StopwatchScreen stopwatchScreen_;
    SlotCatalog slots_{};
    Viewport viewport_{};
    // The clock and the app list, shown whenever no screen is open.
    LauncherController launcher_;
    HomeControlPort* home_=nullptr;
    Screen* active_=nullptr;
    ScreenId activeId_=ScreenId::Home;
    uint32_t homeCount_=0;
    const char* toast_=nullptr;
    TimeUs toastUntil_=INT64_MAX;
};
}
