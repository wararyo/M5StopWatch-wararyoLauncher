#include "ScreenManager.h"
#include "features/launcher/AppListLayout.h"
#include <algorithm>
#include <cmath>
namespace launcher {
ScreenModel ScreenManager::model() const {
    ScreenModel m;
    m.screen=nav_.screen; m.width=nav_.width; m.height=nav_.height;
    m.homeCount=nav_.homeCount; m.toast=nav_.toast;
    m.transition=nav_.transition;
    m.list=list_.state();
    composeHomeRegion(m);
    m.settings=settings_.model();
    m.stats=runtimeSettings_.stats;
    m.external=external_.model();
    m.stopwatch=stopwatchScreen_.model();
    applySlots(slots_,m);
    m.activity=activity();
    return m;
}
FrameActivity ScreenManager::activity() const {
    if (nav_.transition>0 && nav_.transition<1) return FrameActivity::Transition;
    if (active_==&settings_)
        return settings_.active() ? FrameActivity::SettingsScroll : FrameActivity::SettingsSingle;
    if (!active_ && list_.active()) return FrameActivity::LauncherScroll;
    if (active_==&stopwatchScreen_ && stopwatchScreen_.model().state==StopwatchState::Running)
        return FrameActivity::Stopwatch;
    return FrameActivity::Single;
}
bool ScreenManager::open(AppScreen& screen,ScreenId id,TimeUs now) {
    if (!screen.available()) return false;
    // The list keeps its scroll and selection: the screen does not use them, so
    // returning lands back on the same row. Whatever was still moving arrives
    // now, so the hidden list neither draws frames nor shows under the screen.
    list_.finish();
    if (transitionAnimating_) { nav_.transition=toTransition_; stopTransition(); }
    screen.enter(now); active_=&screen; nav_.screen=id;
    return true;
}
void ScreenManager::animateTransition(float transition,TimeUs now) {
    fromTransition_=nav_.transition; toTransition_=transition; transitionStart_=now;
    transitionAnimating_=fromTransition_!=transition;
    transitionFrame_=transitionAnimating_ ? now+ListController::FrameUs : INT64_MAX;
}
bool ScreenManager::update(TimeUs now) {
    now_=now;
    bool changed=false;
    // The runtime's own display deadline is unset while an app screen covers
    // the clock, so a screen that updates on its own gets its frames here.
    if (active_ && now>=active_->nextUpdate()) changed=active_->tick(now) || changed;
    if (nav_.toast && now>=toastUntil_) { nav_.toast=nullptr; toastUntil_=INT64_MAX; changed=true; }
    // The launcher's motion is stopped whenever a screen opens (open()), so
    // this only ever runs while the launcher is what is shown.
    if (!active_ && transitionAnimating_ && now>=transitionFrame_) {
        const float t=std::clamp(float(now-transitionStart_)/float(ListController::AnimationUs),0.0f,1.0f);
        const float eased=1-(1-t)*(1-t)*(1-t);
        nav_.transition=fromTransition_+(toTransition_-fromTransition_)*eased;
        transitionAnimating_=t<1;
        transitionFrame_=transitionAnimating_ ? now+ListController::FrameUs : INT64_MAX;
        changed=true;
    }
    if (!active_) changed=list_.update(now) || changed;
    return changed;
}
TimeUs ScreenManager::nextUpdate() const {
    const TimeUs shown=active_ ? active_->nextUpdate() : std::min(transitionFrame_,list_.nextUpdate());
    return std::min(shown,toastUntil_);
}
bool ScreenManager::handle(const Events& e,TimeUs now) {
    now_=now;
    // A boot commit is not cancellable, so nothing reaches the screen and home
    // itself is suppressed until the API answers (plan.md 8.2 step 3).
    if (active_ && active_->exclusive()) return false;
    if (e.home) {
        const auto homes=nav_.homeCount+1;
        nav_.screen=ScreenId::Home; nav_.toast=nullptr; nav_.transition=0; nav_.homeCount=homes;
        drag_=Drag::None; stopTransition(); toastUntil_=INT64_MAX;
        list_.reset();
        settings_.exit(); external_.exit(); stopwatchScreen_.exit(); active_=nullptr;
        return true;
    }
    bool changed=update(now);
    // An open screen owns its own input. Gestures it does not use simply do
    // nothing, so a stray drag cannot move the list underneath it.
    if (active_) {
        const auto out=active_->handle(e,now);
        if (out.notice) { nav_.toast=out.notice; toastUntil_=now+1400000; }
        if (out.leave) { active_->exit(); active_=nullptr; nav_.screen=ScreenId::AppList; }
        return out.changed || out.leave || out.notice!=nullptr || changed;
    }
    if (e.gesture==Gesture::TouchStart && list_.touchStart()) { stopTransition(); return true; }
    if ((e.gesture==Gesture::Tap || e.gesture==Gesture::DragEnd) && list_.releaseAfterStop(now)) {
        animateTransition(1,now);
        return true;
    }
    if (e.gesture==Gesture::Cancel) {
        drag_=Drag::None;
        list_.cancel(now);
        animateTransition(nav_.screen==ScreenId::Home ? 0 : 1,now);
        return true;
    }
    if (e.gesture==Gesture::DragStart) {
        if (std::abs(e.totalX)>std::abs(e.totalY)) return changed;
        stopTransition();
        dragTransition_=nav_.transition;
        // Decided once, here: the whole drag then goes to this one owner, so
        // a pull that reaches the top never turns from scrolling into home.
        if (nav_.screen==ScreenId::Home) drag_=Drag::Watch;
        else if (list_.atStart() && e.totalY>0) drag_=Drag::Return;
        else drag_=Drag::List;
        if (drag_==Drag::List) list_.dragStart(); else list_.handOff();
    }
    if ((e.gesture==Gesture::DragStart || e.gesture==Gesture::DragMove) && drag_!=Drag::None) {
        const float travel=scaled(nav_.viewport(),170);
        if (drag_==Drag::List) list_.dragMove(float(e.totalY));
        else nav_.transition=std::clamp(dragTransition_-e.totalY/travel,0.0f,1.0f);
        return true;
    }
    if (e.gesture==Gesture::DragEnd && drag_!=Drag::None) {
        const Drag ended=drag_;
        drag_=Drag::None;
        if (ended==Drag::Watch) nav_.screen=e.totalY < -scaled(nav_.viewport(),50) ? ScreenId::AppList : ScreenId::Home;
        if (ended==Drag::Return) nav_.screen=e.totalY > scaled(nav_.viewport(),50) ? ScreenId::Home : ScreenId::AppList;
        if (ended==Drag::List) { list_.dragEnd(-e.velocityY,now); animateTransition(1,now); }
        else { animateTransition(nav_.screen==ScreenId::Home ? 0 : 1,now); list_.align(now); }
        return true;
    }
    // Home alone interrupts an active touch. Buttons may retarget an ongoing
    // animation from its current position, so quick presses are not dropped.
    if (drag_!=Drag::None) return changed;
    const bool tap=e.gesture==Gesture::Tap;
    if (nav_.screen==ScreenId::Home) {
        if (e.next || e.decide || (tap && appsTarget(nav_.viewport()).contains(e.x,e.y))) {
            nav_.screen=ScreenId::AppList; animateTransition(1,now); return true;
        }
        return changed;
    }
    if (e.next) {
        list_.next(now); animateTransition(1,now); return true;
    }
    const bool settling=list_.settling();
    ListDecision decision;
    if (tap) decision=list_.tap(appListPlacement(nav_.viewport(),nav_.transition,list_.scroll()),e.x,e.y,now);
    else if (e.decide) decision=list_.decide(now);
    if (!decision.changed) return changed;
    if (settling) animateTransition(1,now);
    if (!decision.decided) return true;
    if (const AppEntry* entry=appEntry(decision.id)) {
        if (entry->id==AppId::Stopwatch &&
            open(stopwatchScreen_,ScreenId::Stopwatch,now)) return true;
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
    nav_.toast="準備中";
    toastUntil_=now+1400000; return true;
}
}
