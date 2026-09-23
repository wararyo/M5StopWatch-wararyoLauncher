#include "ScreenManager.h"
#include <algorithm>
#include <cmath>
namespace launcher {
ScreenModel ScreenManager::model() const {
    ScreenModel m;
    static_cast<AppListModel&>(m)=model_;
    m.screen=model_.screen; m.width=model_.width; m.height=model_.height;
    m.homeCount=model_.homeCount; m.toast=model_.toast;
    m.animating=animating_;
    composeHomeRegion(m);
    m.settings=settings_.model();
    m.stats=runtimeSettings_.stats;
    m.external=external_.model();
    m.stopwatch=stopwatchScreen_.model();
    // A launchable slot lends the row its own name; everything else keeps the
    // registry name and only dims, so the list stays icon plus name (plan.md
    // 5.2) and the reason lives on the detail screen.
    for (int i=0;i<int(AppRegistry.size());++i) {
        const auto& entry=AppRegistry[i];
        if (entry.kind!=TargetKind::External) continue;
        const auto& slot=slots_.slots[entry.slot-1];
        const bool ready=slots_.layoutSupported && slot.status==SlotStatus::Ready;
        if (ready && slot.name[0]) m.names[i]=slot.name;
        m.rowDimmed[i]=!ready;
    }
    return m;
}
bool ScreenManager::open(AppScreen& screen,ScreenId id,TimeUs now) {
    if (!screen.available()) return false;
    // The list keeps its scroll and selection: the screen does not use them, so
    // returning lands back on the same row.
    screen.enter(now); active_=&screen; model_.screen=id;
    return true;
}
void ScreenManager::animate(float transition,float scroll,TimeUs now) {
    listSettling_=false;
    fromTransition_=model_.transition; fromScroll_=model_.scroll;
    toTransition_=transition; toScroll_=scroll; animationStart_=now;
    animating_=fromTransition_!=transition || fromScroll_!=scroll;
    nextFrame_=animating_ ? now+16000 : INT64_MAX;
}
void ScreenManager::settleList(float velocity,TimeUs now) {
    const float spacing=rowSpacing(model_.viewport());
    // Strong friction: only 90 ms of look-ahead, at most 1.5 extra rows.
    const float projected=model_.scroll+std::clamp(velocity*0.09f,-1.5f*spacing,1.5f*spacing);
    const int target=std::clamp(int(std::lround(projected/spacing)),0,4);
    animate(1,target*spacing,now);
    listSettling_=true;
    const float distance=toScroll_-fromScroll_;
    // Hermite endpoint tangents: release velocity and zero at rest. Limit the
    // initial tangent to keep the curve monotone and inside the list bounds.
    scrollTangent_=distance==0 ? 0 : distance*std::clamp(velocity*0.18f/distance,0.0f,3.0f);
    model_.selection=std::clamp(int(std::lround(model_.scroll/spacing)),0,4);
}
bool ScreenManager::update(TimeUs now) {
    now_=now;
    bool changed=false;
    // The runtime's own display deadline is unset while an app screen covers
    // the clock, so a screen that updates on its own gets its frames here.
    if (active_ && now>=active_->nextUpdate()) changed=active_->tick(now) || changed;
    if (model_.toast && now>=toastUntil_) { model_.toast=nullptr; toastUntil_=INT64_MAX; changed=true; }
    if (animating_ && now>=nextFrame_) {
        const float t=std::clamp(float(now-animationStart_)/180000.0f,0.0f,1.0f);
        const float eased=1-(1-t)*(1-t)*(1-t);
        model_.transition=fromTransition_+(toTransition_-fromTransition_)*eased;
        model_.scroll=fromScroll_+(toScroll_-fromScroll_)*eased;
        if (listSettling_) {
            const float t2=t*t,t3=t2*t;
            model_.scroll=fromScroll_+(toScroll_-fromScroll_)*(3*t2-2*t3)
                +scrollTangent_*(t3-2*t2+t);
            model_.selection=std::clamp(int(std::lround(model_.scroll/rowSpacing(model_.viewport()))),0,4);
        }
        animating_=t<1; nextFrame_=animating_ ? now+16000 : INT64_MAX;
        changed=true;
    }
    return changed;
}
TimeUs ScreenManager::nextUpdate() const {
    return std::min(std::min(nextFrame_,toastUntil_),active_ ? active_->nextUpdate() : INT64_MAX);
}
bool ScreenManager::handle(const Events& e,TimeUs now) {
    now_=now;
    // A boot commit is not cancellable, so nothing reaches the screen and home
    // itself is suppressed until the API answers (plan.md 8.2 step 3).
    if (active_ && active_->exclusive()) return false;
    if (e.home) {
        const int w=model_.width,h=model_.height; const auto homes=model_.homeCount+1;
        model_={}; model_.width=w; model_.height=h; model_.homeCount=homes;
        drag_=Drag::None; animating_=listSettling_=stopTouch_=false; nextFrame_=toastUntil_=INT64_MAX;
        settings_.exit(); external_.exit(); stopwatchScreen_.exit(); active_=nullptr;
        return true;
    }
    bool changed=update(now);
    // An open screen owns its own input. Gestures it does not use simply do
    // nothing, so a stray drag cannot move the list underneath it.
    if (active_) {
        const auto out=active_->handle(e,now);
        if (out.notice) { model_.toast=out.notice; toastUntil_=now+1400000; }
        if (out.leave) { active_->exit(); active_=nullptr; model_.screen=ScreenId::AppList; }
        return out.changed || out.leave || out.notice!=nullptr || changed;
    }
    if (e.gesture==Gesture::TouchStart) {
        stopTouch_=animating_ && listSettling_;
        if (stopTouch_) { animating_=false; nextFrame_=INT64_MAX; return true; }
    }
    if ((e.gesture==Gesture::Tap || e.gesture==Gesture::DragEnd) && stopTouch_) {
        stopTouch_=false;
        settleList(0,now);
        return true;
    }
    if (e.gesture==Gesture::Cancel) {
        stopTouch_=false;
        drag_=Drag::None; model_.dragging=false;
        animate(model_.screen==ScreenId::Home ? 0 : 1,model_.selection*rowSpacing(model_.viewport()),now);
        return true;
    }
    if (e.gesture==Gesture::DragStart) {
        if (std::abs(e.totalX)>std::abs(e.totalY)) return changed;
        stopTouch_=false;
        animating_=false; nextFrame_=INT64_MAX;
        dragScroll_=model_.scroll; dragTransition_=model_.transition;
        drag_=model_.screen==ScreenId::Home ? Drag::Watch :
            (model_.scroll<=0.5f && e.totalY>0 ? Drag::Return : Drag::List);
        model_.dragging=true;
    }
    if ((e.gesture==Gesture::DragStart || e.gesture==Gesture::DragMove) && drag_!=Drag::None) {
        const float travel=scaled(model_.viewport(),170);
        if (drag_==Drag::Watch || drag_==Drag::Return)
            model_.transition=std::clamp(dragTransition_-e.totalY/travel,0.0f,1.0f);
        else {
            model_.scroll=std::clamp(dragScroll_-e.totalY,0.0f,float(4*rowSpacing(model_.viewport())));
            model_.selection=std::clamp(int(std::lround(model_.scroll/rowSpacing(model_.viewport()))),0,4);
        }
        return true;
    }
    if (e.gesture==Gesture::DragEnd && drag_!=Drag::None) {
        const bool list=drag_==Drag::List;
        if (drag_==Drag::Watch) model_.screen=e.totalY < -scaled(model_.viewport(),50) ? ScreenId::AppList : ScreenId::Home;
        if (drag_==Drag::Return) model_.screen=e.totalY > scaled(model_.viewport(),50) ? ScreenId::Home : ScreenId::AppList;
        drag_=Drag::None; model_.dragging=false;
        if (list) settleList(-e.velocityY,now);
        else animate(model_.screen==ScreenId::Home ? 0 : 1,model_.selection*rowSpacing(model_.viewport()),now);
        return true;
    }
    // Home alone interrupts an active touch. Buttons may retarget an ongoing
    // animation from its current position, so quick presses are not dropped.
    if (model_.dragging) return changed;
    const bool tap=e.gesture==Gesture::Tap;
    if (model_.screen==ScreenId::Home) {
        if (e.next || e.decide || (tap && appsTarget(model_.viewport()).contains(e.x,e.y))) {
            model_.screen=ScreenId::AppList; animate(1,model_.scroll,now); return true;
        }
    } else {
        if (e.next) {
            model_.selection=(model_.selection+1)%5;
            animate(1,model_.selection*rowSpacing(model_.viewport()),now); return true;
        }
        const ListGeometry geometry{{model_.width,model_.height},model_.transition,model_.scroll};
        const int row=tap ? hitRow(geometry,e.x,e.y) : -1;
        if (e.decide || row>=0) {
            if (listSettling_ && animating_) animate(1,model_.selection*rowSpacing(model_.viewport()),now);
            if (row>=0) model_.selection=row;
            const auto& entry=AppRegistry[model_.selection];
            if (entry.id==AppId::Stopwatch &&
                open(stopwatchScreen_,ScreenId::Stopwatch,now)) return true;
            if (entry.id==AppId::Settings && open(settings_,ScreenId::Settings,now)) return true;
            // Every external row opens, whatever the slot holds: the detail
            // screen is where an empty or broken slot explains itself.
            if (entry.kind==TargetKind::External) {
                external_.select(entry.slot);
                if (open(external_,ScreenId::External,now)) return true;
            }
            // Reached only when a screen is unavailable because its services
            // were never bound: the input is acknowledged and nothing opens
            // empty. The list itself never advertises a missing target.
            model_.toast="準備中";
            toastUntil_=now+1400000; return true;
        }
    }
    return changed;
}
}
