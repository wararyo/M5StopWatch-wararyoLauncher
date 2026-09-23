#include "ScreenManager.h"
#include <algorithm>
namespace launcher {
FrameModel ScreenManager::model() const {
    FrameModel m;
    m.screen=screen(); m.viewport=viewport_;
    m.homeCount=homeCount_; m.toast=toast_;
    m.launcher=launcher_.model();
    applySlots(slots_,m.launcher);
    m.settings=settings_.model();
    m.stats=runtime_.stats;
    m.external=external_.model();
    m.stopwatch=stopwatchScreen_.model();
    m.activity=activity();
    return m;
}
FrameActivity ScreenManager::activity() const {
    if (launcher_.transitioning()) return FrameActivity::Transition;
    if (active_==&settings_)
        return settings_.active() ? FrameActivity::SettingsScroll : FrameActivity::SettingsSingle;
    if (!active_ && launcher_.scrolling()) return FrameActivity::LauncherScroll;
    if (active_==&stopwatchScreen_ && stopwatchScreen_.model().state==StopwatchState::Running)
        return FrameActivity::Stopwatch;
    return FrameActivity::Single;
}
bool ScreenManager::open(AppScreen& screen,ScreenId id,TimeUs now) {
    if (!screen.available()) return false;
    // The list keeps its scroll and selection: the screen does not use them, so
    // returning lands back on the same row.
    launcher_.suspend();
    screen.enter(now); active_=&screen; activeId_=id;
    return true;
}
bool ScreenManager::launch(const AppEntry* entry,TimeUs now) {
    if (entry) {
        if (entry->id==AppId::Stopwatch && open(stopwatchScreen_,ScreenId::Stopwatch,now)) return true;
        if (entry->id==AppId::Settings && open(settings_,ScreenId::Settings,now)) return true;
        // Every external row opens, whatever the slot holds: the detail
        // screen is where an empty or broken slot explains itself.
        if (entry->kind==TargetKind::External) {
            external_.select(entry->slot);
            if (open(external_,ScreenId::External,now)) return true;
        }
    }
    // Reached only when a screen is unavailable because its services were
    // never bound: the input is acknowledged and nothing opens empty. The
    // list itself never advertises a missing target.
    notify("準備中",now);
    return true;
}
bool ScreenManager::update(TimeUs now) {
    bool changed=false;
    // The runtime's own display deadline is unset while an app screen covers
    // the clock, so a screen that updates on its own gets its frames here.
    if (active_ && now>=active_->nextUpdate()) changed=active_->tick(now) || changed;
    if (toast_ && now>=toastUntil_) { toast_=nullptr; toastUntil_=INT64_MAX; changed=true; }
    // The launcher's motion is stopped whenever a screen opens (open()), so
    // this only ever runs while the launcher is what is shown.
    if (!active_) changed=launcher_.update(now) || changed;
    return changed;
}
TimeUs ScreenManager::nextUpdate() const {
    const TimeUs shown=active_ ? active_->nextUpdate() : launcher_.nextUpdate();
    return std::min(shown,toastUntil_);
}
bool ScreenManager::handle(const Events& e,TimeUs now) {
    // A boot commit is not cancellable, so nothing reaches the screen and home
    // itself is suppressed until the API answers (plan.md 8.2 step 3).
    if (active_ && active_->exclusive()) return false;
    if (e.home) {
        ++homeCount_; toast_=nullptr; toastUntil_=INT64_MAX;
        launcher_.home();
        settings_.exit(); external_.exit(); stopwatchScreen_.exit(); active_=nullptr;
        return true;
    }
    const bool changed=update(now);
    // An open screen owns its own input. Gestures it does not use simply do
    // nothing, so a stray drag cannot move the list underneath it.
    if (active_) {
        const auto out=active_->handle(e,now);
        // The screen only asks; the setting is the application's, and it
        // outlives the screen. It only ever turns on (docs/plan.md 6.3).
        if (out.enableStats) runtime_.stats=true;
        if (out.notice) notify(out.notice,now);
        if (out.leave) { active_->exit(); active_=nullptr; }
        return out.changed || out.leave || out.notice!=nullptr || out.enableStats || changed;
    }
    const auto out=launcher_.handle(e,now);
    if (out.open) return launch(out.target,now);
    return out.changed || changed;
}
}
