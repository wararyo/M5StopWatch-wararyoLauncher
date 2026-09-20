#include "ScreenManager.h"
#include "ui/ListLayout.h"
#include <algorithm>
#include <cmath>
namespace launcher {
void ScreenManager::animate(float transition,float scroll,TimeUs now) {
    listSettling_=false;
    fromTransition_=model_.transition; fromScroll_=model_.scroll;
    toTransition_=transition; toScroll_=scroll; animationStart_=now;
    animating_=fromTransition_!=transition || fromScroll_!=scroll;
    nextFrame_=animating_ ? now+16000 : INT64_MAX;
}
void ScreenManager::settleList(float velocity,TimeUs now) {
    const float spacing=rowSpacing(model_);
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
    bool changed=false;
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
            model_.selection=std::clamp(int(std::lround(model_.scroll/rowSpacing(model_))),0,4);
        }
        animating_=t<1; nextFrame_=animating_ ? now+16000 : INT64_MAX;
        changed=true;
    }
    return changed;
}
TimeUs ScreenManager::nextUpdate() const { return std::min(nextFrame_,toastUntil_); }
bool ScreenManager::handle(const Events& e,TimeUs now) {
    if (e.home) {
        const int w=model_.width,h=model_.height; const auto homes=model_.homeCount+1;
        model_={}; model_.width=w; model_.height=h; model_.homeCount=homes;
        drag_=Drag::None; animating_=listSettling_=stopTouch_=false; nextFrame_=toastUntil_=INT64_MAX;
        return true;
    }
    bool changed=update(now);
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
        animate(model_.screen==ScreenId::Home ? 0 : 1,model_.selection*rowSpacing(model_),now);
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
        const float travel=scaled(model_,170);
        if (drag_==Drag::Watch || drag_==Drag::Return)
            model_.transition=std::clamp(dragTransition_-e.totalY/travel,0.0f,1.0f);
        else {
            model_.scroll=std::clamp(dragScroll_-e.totalY,0.0f,float(4*rowSpacing(model_)));
            model_.selection=std::clamp(int(std::lround(model_.scroll/rowSpacing(model_))),0,4);
        }
        return true;
    }
    if (e.gesture==Gesture::DragEnd && drag_!=Drag::None) {
        const bool list=drag_==Drag::List;
        if (drag_==Drag::Watch) model_.screen=e.totalY < -scaled(model_,50) ? ScreenId::AppList : ScreenId::Home;
        if (drag_==Drag::Return) model_.screen=e.totalY > scaled(model_,50) ? ScreenId::Home : ScreenId::AppList;
        drag_=Drag::None; model_.dragging=false;
        if (list) settleList(-e.velocityY,now);
        else animate(model_.screen==ScreenId::Home ? 0 : 1,model_.selection*rowSpacing(model_),now);
        return true;
    }
    // Home alone interrupts an active touch. Buttons may retarget an ongoing
    // animation from its current position, so quick presses are not dropped.
    if (model_.dragging) return changed;
    const bool tap=e.gesture==Gesture::Tap;
    if (model_.screen==ScreenId::Home) {
        if (e.next || e.decide || (tap && appsTarget(model_).contains(e.x,e.y))) {
            model_.screen=ScreenId::AppList; animate(1,model_.scroll,now); return true;
        }
    } else {
        if (e.next) {
            model_.selection=(model_.selection+1)%5;
            animate(1,model_.selection*rowSpacing(model_),now); return true;
        }
        const int row=tap ? hitRow(model_,e.x,e.y) : -1;
        if (e.decide || row>=0) {
            if (listSettling_ && animating_) animate(1,model_.selection*rowSpacing(model_),now);
            if (row>=0) model_.selection=row;
            // Until tasks 4-6 add the screens, opening an entry only acknowledges
            // the input; the list itself does not advertise the missing targets.
            model_.toast="準備中";
            toastUntil_=now+1400000; return true;
        }
    }
    return changed;
}
}
