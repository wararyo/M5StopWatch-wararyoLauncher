#include "LauncherController.h"
#include "features/launcher/AppListLayout.h"
#include "features/launcher/AppListRows.h"
#include <cmath>
#include <cstdlib>
namespace launcher {
LauncherController::LauncherController(Viewport viewport):viewport_(viewport) {
    list_.resize(viewport);
    // Input reads only the ids, which never change; names are drawn from the
    // frame's model instead.
    list_.setRows(buildAppListRows(AppListModel{},rows_));
}
void LauncherController::suspend() {
    list_.finish();
    if (transitionAnimating_) { transition_=toTransition_; stopTransition(); }
}
void LauncherController::home() {
    listShown_=false; transition_=0; drag_=Drag::None;
    stopTransition();
    list_.reset();
}
AppListModel LauncherController::model() const {
    AppListModel m;
    m.list=list_.state();
    m.transition=transition_;
    return m;
}
void LauncherController::animateTransition(float transition,TimeUs now) {
    fromTransition_=transition_; toTransition_=transition; transitionStart_=now;
    transitionAnimating_=fromTransition_!=transition;
    transitionFrame_=transitionAnimating_ ? now+ListController::FrameUs : INT64_MAX;
}
bool LauncherController::update(TimeUs now) {
    bool changed=false;
    if (transitionAnimating_ && now>=transitionFrame_) {
        const float t=std::clamp(float(now-transitionStart_)/float(ListController::AnimationUs),0.0f,1.0f);
        const float eased=1-(1-t)*(1-t)*(1-t);
        transition_=fromTransition_+(toTransition_-fromTransition_)*eased;
        transitionAnimating_=t<1;
        transitionFrame_=transitionAnimating_ ? now+ListController::FrameUs : INT64_MAX;
        changed=true;
    }
    return list_.update(now) || changed;
}
LauncherOutcome LauncherController::handle(const Events& e,TimeUs now) {
    LauncherOutcome out;
    if (e.gesture==Gesture::TouchStart && list_.touchStart()) { stopTransition(); out.changed=true; return out; }
    if ((e.gesture==Gesture::Tap || e.gesture==Gesture::DragEnd) && list_.releaseAfterStop(now)) {
        animateTransition(1,now);
        out.changed=true; return out;
    }
    if (e.gesture==Gesture::Cancel) {
        drag_=Drag::None;
        list_.cancel(now);
        animateTransition(listShown_ ? 1 : 0,now);
        out.changed=true; return out;
    }
    if (e.gesture==Gesture::DragStart) {
        if (std::abs(e.totalX)>std::abs(e.totalY)) return out;
        stopTransition();
        dragTransition_=transition_;
        // Decided once, here: the whole drag then goes to this one owner, so
        // a pull that reaches the top never turns from scrolling into home.
        if (!listShown_) drag_=Drag::Watch;
        else if (list_.atStart() && e.totalY>0) drag_=Drag::Return;
        else drag_=Drag::List;
        if (drag_==Drag::List) list_.dragStart(); else list_.handOff();
    }
    if ((e.gesture==Gesture::DragStart || e.gesture==Gesture::DragMove) && drag_!=Drag::None) {
        const float travel=scaled(viewport_,170);
        if (drag_==Drag::List) list_.dragMove(float(e.totalY));
        else transition_=std::clamp(dragTransition_-e.totalY/travel,0.0f,1.0f);
        out.changed=true; return out;
    }
    if (e.gesture==Gesture::DragEnd && drag_!=Drag::None) {
        const Drag ended=drag_;
        drag_=Drag::None;
        if (ended==Drag::Watch) listShown_=e.totalY < -scaled(viewport_,50);
        if (ended==Drag::Return) listShown_=!(e.totalY > scaled(viewport_,50));
        if (ended==Drag::List) { list_.dragEnd(-e.velocityY,now); animateTransition(1,now); }
        else { animateTransition(listShown_ ? 1 : 0,now); list_.align(now); }
        out.changed=true; return out;
    }
    // Home alone interrupts an active touch. Buttons may retarget an ongoing
    // animation from its current position, so quick presses are not dropped.
    if (drag_!=Drag::None) return out;
    const bool tap=e.gesture==Gesture::Tap;
    if (!listShown_) {
        if (e.next || e.decide || (tap && appsTarget(viewport_).contains(e.x,e.y))) {
            listShown_=true; animateTransition(1,now); out.changed=true;
        }
        return out;
    }
    if (e.next) {
        list_.next(now); animateTransition(1,now);
        out.changed=true; return out;
    }
    const bool settling=list_.settling();
    ListDecision decision;
    if (tap) decision=list_.tap(appListPlacement(viewport_,transition_,list_.scroll()),e.x,e.y,now);
    else if (e.decide) decision=list_.decide(now);
    if (!decision.changed) return out;
    out.changed=true;
    if (settling) animateTransition(1,now);
    if (!decision.decided) return out;
    // Every row names an entry to open, whatever its slot holds: the external
    // detail screen is where an empty or broken slot explains itself.
    out.open=true; out.target=launchEntry(decision.id);
    return out;
}
}
