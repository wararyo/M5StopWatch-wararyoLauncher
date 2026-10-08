#include "ScreenManager.h"
#include "i18n/Strings.h"
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
    m.timer=timerScreen_.model();
    m.pedometer=pedometerScreen_.model();
    m.homeGesture=gesture_.model();
    m.activity=activity();
    return m;
}
FrameActivity ScreenManager::activity() const {
    if (gesture_.active()) return FrameActivity::HomeGesture;
    if (launcher_.transitioning()) return FrameActivity::Transition;
    if (active_==&settings_)
        return settings_.active() ? FrameActivity::SettingsScroll : FrameActivity::SettingsSingle;
    if (!active_ && launcher_.scrolling()) return FrameActivity::LauncherScroll;
    if (active_==&stopwatchScreen_ && stopwatchScreen_.model().state==StopwatchState::Running)
        return FrameActivity::Stopwatch;
    return FrameActivity::Single;
}
bool ScreenManager::open(Screen& screen,ScreenId id,TimeUs now) {
    if (!screen.available()) return false;
    // The list keeps its scroll and selection: the screen does not use them, so
    // returning lands back on the same row.
    launcher_.suspend();
    screen.enter(now); active_=&screen; activeId_=id;
    return true;
}
bool ScreenManager::launch(const LaunchEntry* entry,TimeUs now) {
    if (entry) {
        if (entry->id==LaunchTargetId::Stopwatch && open(stopwatchScreen_,ScreenId::Stopwatch,now)) return true;
        if (entry->id==LaunchTargetId::Timer && open(timerScreen_,ScreenId::Timer,now)) return true;
        if (entry->id==LaunchTargetId::Pedometer && open(pedometerScreen_,ScreenId::Pedometer,now)) return true;
        if (entry->id==LaunchTargetId::Settings && open(settings_,ScreenId::Settings,now)) return true;
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
    notify(text::Unavailable,now);
    return true;
}
void ScreenManager::leaveAll() {
    toast_=nullptr; toastUntil_=INT64_MAX;
    launcher_.home();
    gesture_.reset();
    settings_.exit(); external_.exit(); stopwatchScreen_.exit(); timerScreen_.exit(); pedometerScreen_.exit();
    active_=nullptr;
}
void ScreenManager::goHome(TimeUs now) {
    ++homeCount_;
    const bool covered=screen()!=ScreenId::Home;
    const float from=gesture_.band();
    leaveAll();
    if (covered) gesture_.reveal(from,now);
}
bool ScreenManager::present(ScreenId id,TimeUs now) {
    Screen* target=nullptr;
    switch (id) {
    case ScreenId::Settings: target=&settings_; break;
    case ScreenId::Stopwatch: target=&stopwatchScreen_; break;
    case ScreenId::Timer: target=&timerScreen_; break;
    case ScreenId::Home: break;
    // The list and the external detail show a choice the request cannot make.
    default: return false;
    }
    if (target && !target->available()) return false;
    // Already shown: entered again, so it takes up what it is asked to show
    // (a countdown that rang) without first being left, which would end it.
    if (target && target==active_) {
        toast_=nullptr; toastUntil_=INT64_MAX; gesture_.reset();
        target->enter(now); return true;
    }
    leaveAll();
    return !target || open(*target,id,now);
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
    changed=gesture_.update(now) || changed;
    return changed;
}
TimeUs ScreenManager::nextUpdate() const {
    const TimeUs shown=active_ ? active_->nextUpdate() : launcher_.nextUpdate();
    return std::min({shown,toastUntil_,gesture_.nextUpdate()});
}
bool ScreenManager::handle(const Events& e,TimeUs now) {
    // A boot commit is not cancellable, so nothing reaches the screen and home
    // itself is suppressed until the API answers (plan.md 8.2 step 3).
    if (active_ && active_->exclusive()) return false;
    // On its way home: nothing is taken, home included (the A+B that fired
    // it is still held), until the clock is in.
    if (gesture_.revealing()) return update(now);
    if (e.home) {
        goHome(now);
        return true;
    }
    bool changed=update(now);
    // The way home by touch, ahead of the screens as home is: a touch at the
    // top edge of an app screen never reaches it.
    const auto gesture=gesture_.handle(e,now,active_!=nullptr,screen()!=ScreenId::Home);
    if (gesture.home) {
        goHome(now);
        return true;
    }
    changed=gesture.changed || changed;
    Events in=e;
    if (gesture.consumed) in.gesture=Gesture::None;
    // An open screen owns its own input. Gestures it does not use simply do
    // nothing, so a stray drag cannot move the list underneath it.
    if (active_) {
        const auto out=active_->handle(in,now);
        // The screen only asks; the setting is the application's, and it
        // outlives the screen. It only ever turns on (docs/plan.md 6.3).
        if (out.enableStats) runtime_.stats=true;
        if (out.notice) notify(out.notice,now);
        if (out.leave) { active_->exit(); active_=nullptr; }
        return out.changed || out.leave || out.notice!=nullptr || out.enableStats || changed;
    }
    // Decided before the launcher sees the event: a tap that lands as a
    // drag or a slide settles is not the clock's.
    const bool atRest=homeAtRest();
    const auto out=launcher_.handle(in,now);
    if (out.open) return launch(out.target,now);
    bool homeChanged=false;
    if (atRest && home_ && (in.gesture==Gesture::Tap || in.gesture==Gesture::LongPress)) {
        HomeEvent event;
        event.kind=in.gesture==Gesture::Tap ? HomeEventKind::Tap : HomeEventKind::LongPress;
        event.x=in.x; event.y=in.y; event.at=now;
        const auto face=home_->handle(event);
        homeChanged=face.changed;
        // The face keeps showing its change; only the saving failed.
        if (face.saveFailed) { notify(text::SaveFailed,now); homeChanged=true; }
        if (face.request==HomeRequest::OpenAppList) homeChanged=launcher_.openList(now) || homeChanged;
    }
    return out.changed || changed || homeChanged;
}
}
